// Translated by: Fabio Olimpieri <fabio.olimpieri@tin.it>
// Updated by: Roberto Togni <see AUTHORS for email address>
// Updated by: PaulTT <see AUTHORS for email address>

// Updated to help_mp-en.h r27402


// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Uso:   mplayer [opzioni] [url|percorso/]nome_file\n"
"\n"
"Opzioni di base: (vedi la pagina man per la lista completa)\n"
" -vo <drv[:dev]>  sceglie driver di uscita video ('-vo help' lista)\n"
" -ao <drv[:dev]>  sceglie driver di uscita audio ('-ao help' lista)\n"
#ifdef CONFIG_VCD
" vcd://<trackno>  legge (S)VCD (Super Video CD) (dispositivo raw, non montato)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<titleno>  legge titolo/traccia DVD dal dispositivo anziché da file\n"
#endif
" -alang/-slang    sceglie lingua audio/sottotitoli DVD (cod naz. 2 caratteri)\n"
" -ss <posizione>  cerca una determinata posizione (in secondi o in hh:mm:ss) \n"
" -nosound         non riproduce l'audio\n"
" -fs              opzioni schermo intero (o -vm, -zoom, vedi pagina man)\n"
" -x <x> -y <y>    imposta la risoluzione dello schermo (usare con -vm o -zoom)\n"
" -sub <file>      file sottotitoli da usare (vedi anche -subfps, -subdelay)\n"
" -playlist <file> specifica il file della playlist\n"
" -vid x -aid y    seleziona il flusso video (x) ed audio (y) da riprodurre\n"
" -fps x -srate y  cambia il rate del video (x fps) e dell'audio (y Hz)\n"
" -pp <quality>    abilita filtro postelaborazione (vedi pagina man x dettagli)\n"
" -framedrop       abilita lo scarto dei fotogrammi (per macchine lente)\n"
"\n"
"Tasti principali: (vedi pagina man per lista, controlla anche input.conf)\n"
" <-  o  ->        va indietro/avanti di 10 secondi\n"
" su o giù         va avanti/indietro di  1 minuto\n"
" pagsu o paggiù   va avanti/indietro di 10 minuti\n"
" < o >            va indietro/avanti nella playlist\n"
" p o SPAZIO       pausa (premere un qualunque tasto per continuare)\n"
" q o ESC          ferma la riproduzione ed esce dal programma\n"
" + o -            regola il ritardo audio di +/- 0.1 secondi\n"
" o                modalità OSD: niente / barra ricerca / barra ricerca + tempo\n"
" * o /            incrementa o decrementa il volume PCM\n"
" x o z            regola il ritardo dei sottotitoli di +/- 0.1 secondi\n"
" r o t            posizione alto/basso dei sottotitoli, vedi anche -vf expand\n"
"\n"
" * * * VEDI PAGINA MAN PER DETTAGLI, ULTERIORI OPZIONI AVANZATE E TASTI! * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
#define MSGTR_SamplesWanted "Servono esempi di questo formato per migliorarne il supporto. Contatta sviluppatori.\n"

// ========================= MPlayer messages ===========================

// mplayer.c:
#define MSGTR_Exiting "\nIn uscita...\n"
#define MSGTR_ExitingHow "\nIn uscita... (%s)\n"
#define MSGTR_Exit_quit "Uscita"
#define MSGTR_Exit_eof "Fine del file"
#define MSGTR_Exit_error "Errore fatale"
#define MSGTR_IntBySignal "\nMPlayer interrotto dal segnale %d nel modulo: %s \n"
#define MSGTR_NoHomeDir "Impossibile trovare la HOME directory\n"
#define MSGTR_GetpathProblem "Problema in get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Creo il file di configurazione: %s\n"
#define MSGTR_BuiltinCodecsConf "Utilizzo la versione interna predefinita di codecs.conf\n"
#define MSGTR_CantLoadFont "Impossibile caricare i font: %s\n"
#define MSGTR_CantLoadSub "Impossibile caricare i sottotitoli: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATALE: manca il flusso selezionato!\n"
#define MSGTR_CantOpenDumpfile "Impossibile aprire il file di dump!!!\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS non specificato (o non valido) nell'intestazione! Usa l'opzione -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Cerco di forzare l'uso della famiglia dei driver dei codec audio %s...\n"
#define MSGTR_CantFindAudioCodec "Impossibile trovare il codec per il formato audio 0x%X!\n"
#define MSGTR_RTFMCodecs "Leggi DOCS/HTML/it/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Cerco di forzare l'uso della famiglia dei driver dei codec video %s...\n"
#define MSGTR_CantFindVideoCodec "Impossibile trovare il codec per il formato video 0x%X!\n"
#define MSGTR_CannotInitVO "FATALE: Impossibile inizializzare il driver video!\n"
#define MSGTR_CannotInitAO "Impossibile aprire/inizializzare il dispositivo audio -> NESSUN SUONO\n"
#define MSGTR_StartPlaying "Inizio la riproduzione...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"       ****************************************************************\n"\
"       **** Il tuo sistema è troppo LENTO per questa riproduzione! ****\n"\
"       ****************************************************************\n"\
"Possibili cause, problemi, soluzioni:\n"\
"- Nella maggior parte dei casi: driver _audio_ danneggiato/bacato\n"\
"  - Prova -ao sdl o usa l'emulazione OSS di ALSA.\n"\
"  - Puoi anche provare con diversi valori di -autosync, 30 è un buon inizio.\n"\
"- Output video lento\n"\
"  - Prova un altro -vo driver (-vo help per la lista) o prova con -framedrop!\n"\
"- CPU lenta\n"\
"  - Non provare a guardare grossi DVD/DivX su CPU lente! Prova qualche opzione\n"\
"di lavdopts, per es.  -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
"- File rovinato\n"\
"  - Prova varie combinazioni di -nobps -ni -forceidx -mc 0.\n"\
"- Dispositivo lento (punti di mount NFS/SMB, DVD, VCD etc)\n"\
"  - Prova -cache 8192.\n"\
"- Stai usando -cache per riprodurre un file AVI senza interleave?\n"\
"  - Prova con -nocache.\n"\
"Leggi DOCS/HTML/it/video.html per suggerimenti su regolazione/accelerazione.\n"\
"Se nulla di ciò ti aiuta, allora leggi DOCS/HTML/it/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer è stato compilato senza il supporto della GUI!\n"
#define MSGTR_GuiNeedsX "La GUI di MPlayer richiede X11!\n"
#define MSGTR_Playing "\nRiproduco %s.\n"
#define MSGTR_NoSound "Audio: nessun suono!!!\n"
#define MSGTR_FPSforced "FPS forzato a %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Compilato con riconoscimento CPU in esecuzione.\n"
#define MSGTR_CompiledWithCPUExtensions "Compilato per CPU x86 con estensioni:"
#define MSGTR_AvailableVideoOutputDrivers "Driver di output video disponibili:\n"
#define MSGTR_AvailableAudioOutputDrivers "Driver di output audio disponibili:\n"
#define MSGTR_AvailableAudioCodecs "Codec audio disponibili:\n"
#define MSGTR_AvailableVideoCodecs "Codec video disponibili:\n"
#define MSGTR_AvailableAudioFm "Famiglie/driver di codec audio disponibili (compilati):\n"
#define MSGTR_AvailableVideoFm "Famiglie/driver di codec video disponibili (compilati):\n"
#define MSGTR_AvailableFsType "Modi disponibili a schermo intero:\n"
#define MSGTR_UsingRTCTiming "Sto utilizzando la temporizzazione hardware RTC di Linux (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: impossibile leggere le proprietà\n"
#define MSGTR_NoStreamFound "Nessun flusso trovato\n"
#define MSGTR_ErrorInitializingVODevice "Errore aprendo/inizializzando il dispositivo uscita video (-vo) selezionato!\n"
#define MSGTR_ForcedVideoCodec "Codec video forzato: %s\n"
#define MSGTR_ForcedAudioCodec "Codec audio forzato: %s\n"
#define MSGTR_Video_NoVideo "Video: nessun video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATALE: Impossibile inizializzare i filtri video (-vf) o l'output video (-vo)!\n"
#define MSGTR_Paused "\n  =====  PAUSA  =====\r"
#define MSGTR_PlaylistLoadUnable "\nImpossibile caricare la playlist %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer è stato interrotto dal segnale 'Istruzione illegale'.\n"\
"  Potrebbe essere un errore nel codice di rilevamento tipo di processore...\n"\
"  leggi DOCS/HTML/it/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer è stato interrotto dal segnale 'Istruzione illegale'.\n"\
"  Solitamente questo avviene quando si esegue il programma su un processore\n"\
"  diverso da quello per cui è stato compilato/ottimizzato.\n"\
"  Verificalo!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer è stato interrotto per un errore nell'uso della CPU/FPU/RAM.\n"\
"  Ricompila MPlayer con --enable-debug e crea un backtrace ed un disassemblato\n"\
"  con 'gdb'. Per dettagli DOCS/HTML/it/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer è andato in crash. Questo non dovrebbe accadere.\n"\
"  Può essere un errore nel codice di MPlayer _o_ nei tuoi driver _o_ nella tua\n"\
"  versione di gcc. Se ritieni sia colpa di MPlayer, per favore leggi\n"\
"  DOCS/HTML/it/bugreports.html e segui quelle istruzioni. Non possiamo\n"\
"  aiutarti, e non lo faremo, se non ci dai queste informazioni quando segnali\n"\
"  un possibile problema.\n"
#define MSGTR_LoadingConfig "Carico configurazione '%s'\n"
#define MSGTR_LoadingProtocolProfile "Carico il profilo '%s' riguardante il protocollo\n"
#define MSGTR_LoadingExtensionProfile "Carico il profilo '%s' riguardante l'estensione\n"
#define MSGTR_AddedSubtitleFile "SUB: Aggiunto file sottotitoli (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: Rimosso file sottotitoli (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Errore durante l'apertura del file [%s] per la scrittura!\n"
#define MSGTR_CommandLine "CommandLine:"
#define MSGTR_RTCDeviceNotOpenable "Apertura di %s fallita: %s (dovrebbe esser leggibile dall'utente.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC: errore di init in ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Prova aggiungendo \"echo %lu > /proc/sys/dev/rtc/max-user-freq\"\n"\
"agli script di avvio del sistema.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC: errore di init in ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Uso la temporizzazione %s.\n"
#define MSGTR_NoIdleAndGui "L'opzione -idle non può essere usata con GMPlayer.\n"
#define MSGTR_MenuInitialized "Menu inizializzato: %s\n"
#define MSGTR_MenuInitFailed "Inizializzazione Menu fallita.\n"
#define MSGTR_Getch2InitializedTwice "WARNING: getch2_init chiamata 2 volte!\n"
#define MSGTR_DumpstreamFdUnavailable "Non posso fare il dump di questo flusso - nessun descrittore file disponibile.\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Non riesco ad aprire il filtro video libmenu col menu base %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Errore nel pre-init della sequenza di filtri audio!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC: errore di lettura: %s\n"
// TODO: softsleep underflow ???
#define MSGTR_SoftsleepUnderflow "Attenzione! Softsleep underflow!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV Evento NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV Evento: Evento Highlight bacato\n"
#define MSGTR_DvdnavEvent "DVDNAV Evento: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV Evento: Nascondo Highlight\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV Evento: Still Frame: %d sec(s)\n"
#define MSGTR_DvdnavNavStop "DVDNAV Evento: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "DVDNAV Evento: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV Evento: Nav Cambio Flusso SPU: phys: %d/%d/%d logico: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV Evento: Nav Cambio Flusso SPU: phys: %d logico: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV Evento: Nav Cambio Flusso Audio: phys: %d logico: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV Evento: Nav Cambio VTS\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV Evento: Nav Cambio Cella\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV Evento: Nav Cambio CLUT SPU\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV Evento: Nev Ricerca eseguita\n"
#define MSGTR_MenuCall "Chiamata a Menu\n"

#define MSGTR_EdlOutOfMem "Non posso allocare abbastanza memoria per i dati EDL.\n"
#define MSGTR_EdlRecordsNo "Lette azioni EDL %d.\n"
#define MSGTR_EdlQueueEmpty "Non ci sono azioni EDL di cui curarsi.\n"
#define MSGTR_EdlCantOpenForWrite "Non posso aprire il file EDL [%s] per la scrittura.\n"
#define MSGTR_EdlCantOpenForRead "Non posso aprire il file EDL [%s] per la lettura.\n"
#define MSGTR_EdlNOsh_video "Non posso usare EDL senza video, disabilitate.\n"
#define MSGTR_EdlNOValidLine "Linea EDL invalida: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Linea EDL scritta male [%d], la ignoro.\n"
#define MSGTR_EdlBadLineOverlap "L'ultimo stop era a [%f]; lo start successivo a [%f].\n"\
"Le indicazioni devono essere in ordine, non posso sovrapporle. Ignoro.\n"
#define MSGTR_EdlBadLineBadStop "Lo stop deve essere dopo il tempo di start.\n"
#define MSGTR_EdloutBadStop "Salto EDL ingnorato, ultimo start > stop\n"
#define MSGTR_EdloutStartSkip "Tempo di start EDL impostato, ripremere 'i' per impostare lo stop.\n"
#define MSGTR_EdloutEndSkip "Fine del blocco EDL, riga scritta.\n"
#define MSGTR_MPEndposNoSizeBased "L'opzione -endpos in MPlayer non supporta ancora unità di mis. di dimensione.\n"

// mplayer.c OSD
#define MSGTR_OSDenabled "abilitat"
#define MSGTR_OSDdisabled "disabilitat"
#define MSGTR_OSDAudio "Audio: %s"
#define MSGTR_OSDChannel "Canale: %s"
#define MSGTR_OSDSubDelay "Ritardo sottotitoli: %d ms"
#define MSGTR_OSDSpeed "Velocità: x %6.2f"
#define MSGTR_OSDosd "OSD: %so"
#define MSGTR_OSDChapter "Capitolo: (%d) %s"
#define MSGTR_OSDAngle "Angolazione: %d/%d"

// property values
#define MSGTR_Enabled "abilitat"
#define MSGTR_EnabledEdl "abilitato (EDL)"
#define MSGTR_Disabled "disabilitat"
#define MSGTR_HardFrameDrop "hard - intens"
#define MSGTR_Unknown "sconosciuto"
#define MSGTR_Bottom "in basso"
#define MSGTR_Center "al centro"
#define MSGTR_Top "in alto"
#define MSGTR_SubSourceDemux "incorporati"

// OSD bar names
#define MSGTR_Volume "Volume"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "Luminosità"
#define MSGTR_Contrast "Contrasto"
#define MSGTR_Saturation "Saturazione"
#define MSGTR_Hue "Tonalità"
#define MSGTR_Balance "Bilanciamento"

// property state
#define MSGTR_LoopStatus "Ripetizione: %s"
#define MSGTR_MuteStatus "Muto: %so"
#define MSGTR_AVDelayStatus "Ritardo A-V: %s"
#define MSGTR_OnTopStatus "Resta in primo piano: %so"
#define MSGTR_RootwinStatus "Rootwin: %so"
#define MSGTR_BorderStatus "Bordo: %s"
#define MSGTR_FramedroppingStatus "Scarto fotogrammi: %so"
#define MSGTR_VSyncStatus "VSync: %s"
#define MSGTR_SubSelectStatus "Sottotitoli: %s"
#define MSGTR_SubSourceStatus "Origine sottotitoli: %s"
#define MSGTR_SubPosStatus "Posizione sottotitoli: %s/100"
#define MSGTR_SubAlignStatus "Allineamento sottotitoli: %s"
#define MSGTR_SubDelayStatus "Ritardo sottotitoli: %s"
#define MSGTR_SubScale "Dimensione sottotitoli: %s"
#define MSGTR_SubVisibleStatus "Sottotitoli: %si"
#define MSGTR_SubForcedOnlyStatus "Solo sottotitoli forzati: %si"

// mencoder.c:
#define MSGTR_UsingPass3ControlFile "Sto usando il file di controllo passo3: %s\n"
#define MSGTR_MissingFilename "\nNome file mancante.\n\n"
#define MSGTR_CannotOpenFile_Device "Impossibile aprire il file/dispositivo.\n"
#define MSGTR_CannotOpenDemuxer "Impossibile aprire il demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nNessun encoder audio (-oac) scelto! Selezionane uno (vedi -oac help) o -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nNessun encoder video (-ovc) scelto! Selezionane uno (vedi -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Impossibile aprire il file di output '%s'.\n"
#define MSGTR_EncoderOpenFailed "Errore nell'apertura dell'encoder.\n"
#define MSGTR_MencoderWrongFormatAVI "\nATTENZIONE: IL FORMATO DEL FILE DI OUTPUT è _AVI_. Vedi -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nATTENZIONE: IL FORMATO DEL FILE DI OUTPUT è _MPEG_. Vedi -of help.\n"
#define MSGTR_MissingOutputFilename "Nessun file di output specificato, per favore verifica l'opzione -o."
#define MSGTR_ForcingOutputFourcc "Forzo il fourcc di output a %x [%.4s].\n"
#define MSGTR_ForcingOutputAudiofmtTag "Forzo la tag del formato audio a 0x%x.\n"
#define MSGTR_DuplicateFrames "\n%d fotogramma/i duplicato/i!!!    \n"
#define MSGTR_SkipFrame "\nScarto fotogramma!\n"
#define MSGTR_ResolutionDoesntMatch "\nIl nuovo file video ha diversa risoluzione o spazio colore dal precedente.\n"
#define MSGTR_FrameCopyFileMismatch "\nTutti i file video devono avere stessi fps, risoluz., e codec per -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nTutti i file devono avere lo stesso codec audio e formato per -oac copy.\n"
#define MSGTR_NoAudioFileMismatch "\nImpossibile mescolare file solo audio con file video. Prova con -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "WARNING: -speed non è detto che funzioni correttamente con -oac copy!\n"\
"La codifica potrebbe risultare danneggiata!\n"
#define MSGTR_ErrorWritingFile "%s: errore nella scrittura del file.\n"
#define MSGTR_FlushingVideoFrames "\nScaricamento fotogrammi video.\n"
#define MSGTR_FiltersHaveNotBeenConfiguredEmptyFile "I filtri non son stati configurati! File vuoto?\n"
#define MSGTR_RecommendedVideoBitrate "Il bitrate video consigliato per %s CD è: %d\n"
#define MSGTR_VideoStreamResult "\nFlusso video: %8.3f kbit/s  (%d B/s)  dim.: %"PRIu64" byte  %5.3f sec  %d fotogrammi\n"
#define MSGTR_AudioStreamResult "\nFlusso audio: %8.3f kbit/s  (%d B/s)  dim.: %"PRIu64" byte  %5.3f secondi\n"
#define MSGTR_OpenedStream "successo: formato: %d  dati: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "CBR PCM audio selezionato.\n"
#define MSGTR_MP3AudioSelected "MP3 audio selezionato.\n"
#define MSGTR_CannotAllocateBytes "Non posso allocare %d byte.\n"
#define MSGTR_SettingAudioDelay "Imposto il ritardo audio a %5.3f.\n"
#define MSGTR_SettingVideoDelay "Imposto il ritardo video a %5.3f.\n"
#define MSGTR_SettingAudioInputGain "Imposto il guadagno di ingresso audio a %f.\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Limito il preload audio a 0.4s.\n"
#define MSGTR_IncreasingAudioDensity "Aumento la densità audio a 4.\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Forzo il preload audio a 0, max pts correction a 0.\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %d byte/sec, %d byte/blocco\n"
#define MSGTR_LameVersion "LAME versione %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset ""\
"Errore: il bitrate specificato è fuori gamma per questo Preset.\n"\
"\n"\
"Quando usi questo metodo devi usare un valore tra \"8\" e \"320\".\n"\
"\n"\
"Per altre informazioni usa: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions ""\
"Errore: immesso un profilo e/o delle opzioni errate per questo Preset.\n"\
"\n"\
"I profili disponibili sono:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - La modalità ABR è implicita. Per usarla,\n"\
"                      specifica un bitrate. Per esempio:\n"\
"                      \"preset=185\" attiva questo\n"\
"                      preset e usa 185 come kbps medi.\n"\
"\n"\
"    Qualche esempio:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" o  \"-lameopts  cbr:preset=192       \"\n"\
" o  \"-lameopts      preset=172       \"\n"\
" o  \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Per altre informazioni usa: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"I Presets sono costruiti in modo da dare la più alta qualità possibile.\n"\
"\n"\
"Sono stati per la maggior parte sottosposti a test e rifiniti attraverso\n"\
"doppi test di ascolto per verificare e ottenere tale obiettivo.\n"\
"\n"\
"Vengono aggiornati continuamente per coincidere con gli ultimi sviluppi che\n"\
"ci sono e come risultato dovrebbero dare probabilmente la miglior qualità\n"\
"attualmente possibile con LAME.\n"\
"\n"\
"Per attivare questi Presets:\n"\
"\n"\
"   Per le modalità VBR (di solito qualità più alta):\n"\
"\n"\
"     \"preset=standard\" Questo Preset di solito dovrebbe essere trasparente\n"\
"                             per molte persone per molta musica ed è già\n"\
"                             di qualità piuttosto alta.\n"\
"\n"\
"     \"preset=extreme\"  Se hai una sensibilità sonora buona e equivalente\n"\
"                             equipaggiamento, questo Preset avrà solitamente\n"\
"                             una qualità un po' più alta della modalità\n"\
"                             \"standard\".\n"\
"\n"\
"   Per modalità CBR a 320kbps (la qualità più alta possibile per i Presets):\n"\
"\n"\
"     \"preset=insane\"   Questo Preset dovrebbe essere decisamente buono\n"\
"                             per la maggior parte di persone e situazioni,\n"\
"                             ma se devi avere assoluta alta qualità e nessun\n"\
"                             rispetto per la dimensione, devi usare questo.\n"\
"\n"\
"   Per modalità ABR (alta qualità per dato bitrate ma non alta come VBR):\n"\
"\n"\
"     \"preset=<kbps>\"   Usare questo Preset darà solitamente buona qualità\n"\
"                             a un dato bitrate. In dipendenza dal bitrate\n"\
"                             indicato, questo Preset determinerà ottimali\n"\
"                             impostazioni per quella particolare situazione.\n"\
"                             Anche se questo approccio funge, non è per\n"\
"                             niente flessibile come VBR, e di solito non dà\n"\
"                             la stessa qualità di VBR a bitrate più alti.\n"\
"\n"\
"Le seguenti opzioni sono disponibili anche per i corrispondenti profili:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Modalità ABR) - La modalità ABR è implicita. Per usarla,\n"\
"                   indicare semplicemente un bitrate. Per esempio:\n"\
"                   \"preset=185\" attiva questo Preset e viene\n"\
"                   usato 185 come media kbps.\n"\
"\n"\
"   \"fast\" - Abilita il nuovo VBR \"veloce\" per un dato profilo. Lo\n"\
"            svantaggio dell'alta velocità è che spesso il bitrate\n"\
"            risulta leggermente più alto rispetto alla modalità normale\n"\
"            e la qualità leggermente inferiore.\n"\
"   Attenzione: nell'attuale versione l'utilizzo di Preset \"veloce\" può\n"\
"            portare un bitrate troppo alto del normale.\n"\
"\n"\
"   \"cbr\"  - se usi la modalità ABR (leggi sopra) con un certo bitrate\n"\
"            significativo come 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            puoi usare l'opzione \"cbr\" per forzare la codifica in modalità\n"\
"            CBR al posto dello standard abr. ABR dà una più alta qualità,\n"\
"            ma CBR torna utile in quelle situazioni dove ad esempio\n"\
"            trasmettere un MP3 su internet può essere importante.\n"\
"\n"\
"    Per esempio:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" o  \"-lameopts  cbr:preset=192       \"\n"\
" o  \"-lameopts      preset=172       \"\n"\
" o  \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Ci sono alcuni sinonimi per le modalità ABR:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit \
"Non posso impostare le opzioni di LAME, controlla bitrate/samplerate,\n"\
"per bitrate molto bassi (<32) servono minori samplerate (es. -srate 8000).\n"\
"Se ogni altra cosa non funziona, prova un Preset."
#define MSGTR_ConfigFileError "errore file di configurazione"
#define MSGTR_ErrorParsingCommandLine "errore leggendo la riga comando"
#define MSGTR_VideoStreamRequired "Il flusso video è obbligatorio!\n"
#define MSGTR_ForcingInputFPS "i fps saranno interpretati come %5.3f.\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Il formato output RAWVIDEO non supporta l'audio - lo disabilito.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Questo demuxer non supporta ancora -nosound.\n"
#define MSGTR_MemAllocFailed "Allocazione memoria non riuscita.\n"
#define MSGTR_NoMatchingFilter "Non trovo il filtro/il formato ao corrispondente!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, compilatore C bacato?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, Manca il nome del codec!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, Non trovo l'encoder per il codec %s.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, non posso allocare il contesto!\n"
#define MSGTR_CouldntOpenCodec "Non posso aprire il codec %s, br=%d.\n"
#define MSGTR_CantCopyAudioFormat "Il formato audio 0x%x è incompatibile con '-oac copy', prova invece '-oac pcm' o usa '-fafmttag' per forzare.\n"

// cfg-mencoder.h:
#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metodo bitrate variabile\n"\
"                0: cbr (bitrate costante)\n"\
"                1: mt (algoritmo VBR di Mark Taylor)\n"\
"                2: rh (algoritmo VBR di Robert Hegemann - default)\n"\
"                3: abr (bitrate medio)\n"\
"                4: mtrh (algoritmo VBR di Mark Taylor Robert Hegemann)\n"\
"\n"\
" abr           bitrate medio\n"\
"\n"\
" cbr           bitrate costante\n"\
"               Forza il metodo CBR anche sui successivi Preset ABR\n"\
"\n"\
" br=<0-1024>   specifica il bitrate in kBit (solo CBR e ABR)\n"\
"\n"\
" q=<0-9>       qualità (0-massima, 9-minima) (solo per VBR)\n"\
"\n"\
" aq=<0-9>      qualità algoritmo (0-migliore/più lento, 9-peggiore/più veloce)\n"\
"\n"\
" ratio=<1-100> rapporto di compressione\n"\
"\n"\
" vol=<0-10>    imposta il guadagno dell'ingresso audio\n"\
"\n"\
" mode=<0-3>    (default: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: due canali indipendenti\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: no\n"\
"                1: tutto\n"\
"                2: regola\n"\
"\n"\
" fast          attiva la codifica più veloce sui successivi preset VBR,\n"\
"               qualità leggermente inferiore ai bitrate più alti.\n"\
"\n"\
" preset=<value> fornisce le migliori impostazioni possibili di qualità.\n"\
"                 medium: codifica VBR, buona qualità\n"\
"                 (intervallo bitrate 150-180 kbps)\n"\
"                 standard:  codifica VBR, qualità alta\n"\
"                 (intervallo bitrate 170-210 kbps)\n"\
"                 extreme: codifica VBR, qualità molto alta\n"\
"                 (intervallo bitrate 200-240 kbps)\n"\
"                 insane:  codifica CBR, massima qualità via preset\n"\
"                 (bitrate 320 kbps)\n"\
"                 <8-320>: codifica ABR con bitrate medio impostato in kbps.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "FourCC duplicato"
#define MSGTR_TooManyFourccs "troppi FourCCs/formati..."
#define MSGTR_ParseError "errore lettura"
#define MSGTR_ParseErrorFIDNotNumber "errore lettura (ID formato non è un numero?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "errore lettura (l'alias ID formato non è un numero?)"
#define MSGTR_DuplicateFID "ID formato duplicato"
#define MSGTR_TooManyOut "troppi out..."
#define MSGTR_InvalidCodecName "\nnome codec(%s) non valido!\n"
#define MSGTR_CodecLacksFourcc "\nil codec(%s) non ha FourCC/formato!\n"
#define MSGTR_CodecLacksDriver "\nil codec(%s) non ha un driver!\n"
#define MSGTR_CodecNeedsDLL "\nil codec(%s) abbisogna di una 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nil codec(%s) abbisogna di un 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Non riesco ad allocare memoria per il commento."
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Leggo %s: "
#define MSGTR_CantOpenFileError "Non posso aprire '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Non posso aver la memoria per 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Non posso riallocare '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Il nome codec '%s' non è univoco."
#define MSGTR_CantStrdupName "Non posso far strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Non posso far strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Non posso far strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Non posso far strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audio & %d video codecs\n"
#define MSGTR_CodecDefinitionIncorrect "Il codec non è correttamente definito."
#define MSGTR_OutdatedCodecsConf "Il codecs.conf è troppo vecchio/incompatibile con questa versione di MPlayer!"

// fifo.c
#define MSGTR_CannotMakePipe "Non posso costruire una PIPE!\n"

// parser-mecmd.c, parser-mpcmd.c
#define MSGTR_NoFileGivenOnCommandLine "'--' indica la fine opzioni, ma nessun nome file è stato fornito sulla riga comando.\n"
#define MSGTR_TheLoopOptionMustBeAnInteger "L'opzione loop deve essere un numero intero: %s\n"
#define MSGTR_UnknownOptionOnCommandLine "Opzione sconosciuta sulla riga comando: -%s\n"
#define MSGTR_ErrorParsingOptionOnCommandLine "Errore durante la lettura opzioni della riga comando: -%s\n"
#define MSGTR_InvalidPlayEntry "Voce da riprodurre %s non valida\n"
#define MSGTR_NotAnMEncoderOption "-%s non è un'opzione di MEncoder\n"
#define MSGTR_NoFileGiven "Nessun file fornito\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Lo slot salvato da lvl %d è troppo vecchio: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "L'opzione %s non si può usare nel file di configurazione.\n"
#define MSGTR_InvalidCmdlineOption "L'opzione %s non si può usare da riga comando.\n"
#define MSGTR_InvalidSuboption "Errore: l'opzione '%s' non ha la sottoopzione '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Errore: la sottoopzione '%s' di '%s' deve avere un parametro!\n"
#define MSGTR_MissingOptionParameter "Errore: l'opzione '%s' deve avere un parametro!\n"
#define MSGTR_OptionListHeader "\n Nome                 Tipo            Min        Max      Global  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nTotale: %d opzioni\n"
#define MSGTR_ProfileInclusionTooDeep "ATTENZIONE: Livello di inclusione troppo profondo nel profilo.\n"
#define MSGTR_NoProfileDefined "Non è stato definito alcun profilo.\n"
#define MSGTR_AvailableProfiles "Profili disponibili:\n"
#define MSGTR_UnknownProfile "Profilo '%s' sconosciuto.\n"
#define MSGTR_Profile "Profilo %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Nome                 Tipo            Min        Max\n\n"
#define MSGTR_TotalProperties "\nTotale: %d proprietà\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo CD-ROM '%s' non trovato!\n"
#define MSGTR_ErrTrackSelect "Errore nella selezione della traccia del VCD!"
#define MSGTR_ReadSTDIN "Leggo da stdin...\n"
#define MSGTR_UnableOpenURL "Impossibile aprire l'URL: %s\n"
#define MSGTR_ConnToServer "Connesso al server: %s\n"
#define MSGTR_FileNotFound "File non trovato: '%s'\n"

#define MSGTR_SMBInitError "Impossibile inizializzare la libreria libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Impossibile aprire dalla rete: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer non è stato compilato con supporto di lettura da SMB.\n"

// stream_dvd.c
#define MSGTR_DVDspeedCantOpen "Impossibile aprire il dispositivo DVD in scrittura, modificarne la velocità\nrichiede accesso in scrittura.\n"
#define MSGTR_DVDrestoreSpeed "Ripristino la velocità del DVD... "
#define MSGTR_DVDlimitSpeed "Limitazione della velocità del DVD a %dKB/s... "
#define MSGTR_DVDlimitFail "non riuscita\n"
#define MSGTR_DVDlimitOk "riuscita\n"
#define MSGTR_CantOpenDVD "Impossibile aprire il dispositivo DVD: %s (%s)\n"
#define MSGTR_NoDVDSupport "MPlayer è stato compilato senza il supporto per DVD, esco\n"
#define MSGTR_DVDnumTitles "Ci sono %d titolo/i su questo DVD.\n"
#define MSGTR_DVDinvalidTitle "Numero del titolo del DVD non valido: %d\n"
#define MSGTR_DVDnumChapters "Ci sono %d capitolo/i in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidChapter "Numero del capitolo del DVD non valido: %d\n"
#define MSGTR_DVDinvalidChapterRange "Intervallo dei capitoli indicato non valido: %s\n"
#define MSGTR_DVDinvalidLastChapter "Numero del capitolo finale del DVD non valido: %d\n"
#define MSGTR_DVDnumAngles "Ci sono %d angolazione/i in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidAngle "Numero delle angolazioni del DVD non valido: %d\n"
#define MSGTR_DVDnoIFO "Impossibile aprire il file IFO per il titolo del DVD %d.\n"
#define MSGTR_DVDnoVMG "Impossibile aprire le informazioni VMG!\n"
#define MSGTR_DVDnoVOBs "Impossibile aprire il VOB del titolo (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "Non trovata la lingua dell'audio DVD corrispondente!\n"
#define MSGTR_DVDaudioChannel "Scelto canale audio DVD: %d lingua: %c%c\n" 
#define MSGTR_DVDaudioStreamInfo "flusso audio: %d formato: %s (%s) lingua: %s aid: %d.\n"
#define MSGTR_DVDnumAudioChannels "numero di canali audio sul disco: %d.\n"
#define MSGTR_DVDnoMatchingSubtitle "Non trovata la lingua dei sottotitoli DVD corrispondente!\n"
#define MSGTR_DVDsubtitleChannel "Scelto canale sottotitoli DVD: %d lingua: %c%c\n"
#define MSGTR_DVDsubtitleLanguage "sottotitoli ( sid ): %d lingua: %s\n"
#define MSGTR_DVDnumSubtitles "numero di sottotitoli sul disco: %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Troppi flussi!"
#define MSGTR_RawMuxerOnlyOneStream "Il muxer rawaudio supporta solo un flusso audio!\n"
#define MSGTR_IgnoringVideoStream "Ignoro il flusso video!\n"
#define MSGTR_UnknownStreamType "Attenzione, Tipo flusso sconosciuto: %d\n"
#define MSGTR_WarningLenIsntDivisible "Attenzione, len non è divisibile per samplesize!\n"
// TODO: muxer frame buffer ???
#define MSGTR_MuxbufMallocErr "Il buffer fotogrammi del muxer non può allocare la memoria!\n"
#define MSGTR_MuxbufReallocErr "Il buffer fotogrammi del muxer non può riallocare la memoria!\n"
#define MSGTR_MuxbufSending "Il buffer fotogrammi del muxer sta inviando %d fotogramma/i al muxer.\n"
#define MSGTR_WritingHeader "Scrittura intestazione in corso...\n"
#define MSGTR_WritingTrailer "Scrittura indice in corso...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Avvertimento! Intestazione del flusso audio %d ridefinito!\n"
#define MSGTR_VideoStreamRedefined "Avvertimento! Intestazione del flusso video %d ridefinito!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti audio nel buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti video nel buffer!\n"
#define MSGTR_MaybeNI "Forse stai riproducendo un flusso/file non-interleaved o il codec non funziona?\n" \
          "Per i file .AVI, prova a forzare la modalità 'non-interleaved' con l'opz. -ni.\n"
// TODO XXX working around
#define MSGTR_WorkAroundBlockAlignHeaderBug "AVI: XXX il bug dell'intestazione nBlockAlign CBR-MP3!\n"
#define MSGTR_SwitchToNi "\nRilevato file .AVI con interleave errato - passo alla modalità -ni!\n"
#define MSGTR_InvalidAudioStreamNosound "AVI: ID flusso audio invalido: %d - lo ignoro (nosound)\n"
#define MSGTR_InvalidAudioStreamUsingDefault "AVI: ID flusso video invalido: %d - lo ignoro (uso il default)\n"
#define MSGTR_ON2AviFormat "Formato AVI ON2"
#define MSGTR_Detected_XXX_FileFormat "Rilevato formato file %s!\n"
#define MSGTR_DetectedAudiofile "Rilevato file audio!\n"
#define MSGTR_NotSystemStream "il formato non è 'MPEG System Stream'... (è forse 'Transport Stream'?)\n"
#define MSGTR_InvalidMPEGES "Flusso MPEG-ES non valido??? Contatta l'autore, può essere un baco :(\n"
#define MSGTR_FormatNotRecognized "===== Mi dispiace, questo formato file non è riconosciuto/supportato ======\n"\
				  "=== Se questo è un file AVI, ASF o MPEG, per favore contatta l'autore! ===\n"
#define MSGTR_SettingProcessPriority "Imposto la priorità del processo: %s\n"
#define MSGTR_CannotInitializeMuxer "Impossibile inizializzare il muxer."
#define MSGTR_MissingVideoStream "Nessun flusso video trovato!\n"
#define MSGTR_MissingAudioStream "Nessun flusso audio trovato -> nessun suono\n"
#define MSGTR_MissingVideoStreamBug "Manca il flusso video!? Contatta l'autore, può essere un baco :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: il file non contiene il flusso audio o video selezionato\n"

#define MSGTR_NI_Forced "Forzato"
#define MSGTR_NI_Detected "Rilevato"
#define MSGTR_NI_Message "%s formato file AVI NON-INTERLEAVED!\n"

#define MSGTR_UsingNINI "Uso di formato file AVI NON-INTERLEAVED corrotto.\n"
#define MSGTR_CouldntDetFNo "Impossibile determinare il numero di fotogrammi (per lo spostamento assoluto).\n"
#define MSGTR_CantSeekRawAVI "Impossibile spostarsi nei flussi .AVI grezzi. (richiesto un indice, prova con l'opzione -idx.)\n"
#define MSGTR_CantSeekFile "Impossibile spostarsi in questo file!  \n"

#define MSGTR_MOVcomprhdr "MOV: Il supporto delle intestazioni compresse richiede ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: Avvertimento! Rilevato FourCC variabile!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Avvertimento! troppe tracce!"
#define MSGTR_FoundAudioStream "==> Trovato flusso audio: %d\n"
#define MSGTR_FoundVideoStream "==> Trovato flusso video: %d\n"
#define MSGTR_DetectedTV "Ho trovato una TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Impossibile aprire il demuxer Ogg.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: sto cercando il flusso audio (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Impossibile aprire il flusso audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Impossibile aprire il flusso dei sottotitoli: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Errore nell'apertura del demuxer audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Errore nell'apertura del demuxer dei sottotitoli: %s\n"
#define MSGTR_TVInputNotSeekable "Impossibile spostarsi in un programma TV!\n"\
"(Probabilmente lo spostamento verrà usato per cambiare canale ;)\n"
#define MSGTR_DemuxerInfoChanged "Info demuxer %s cambiate a %s\n"
#define MSGTR_ClipInfo "Informazioni file multimediale:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Rilevato formato NTSC 30000/1001fps, cambio framerate.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: Rilevato formato NTSC 24000/1001fps progressivo, cambio framerate.\n"

#define MSGTR_CacheFill "\rRiempio cache: %5.2f%% (%"PRId64" byte)   " 
#define MSGTR_NoBindFound "Nessun controllo legato al tasto '%s'."
#define MSGTR_FailedToOpen "Apertura di '%s' fallita.\n"
							       //
#define MSGTR_VideoID "[%s] Trovato flusso video, -vid %d\n"
#define MSGTR_AudioID "[%s] Trovato flusso audio, -aid %d\n"
#define MSGTR_SubtitleID "[%s] Trovato flusso sottotitoli, -sid %d\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "impossibile aprire il codec\n"
#define MSGTR_CantCloseCodec "impossibile chiudere il codec\n"

#define MSGTR_MissingDLLcodec "ERRORE: Impossibile aprire il codec DirectShow richiesto: %s\n"
#define MSGTR_ACMiniterror "Impossibile caricare/inizializz. il codec audio Win32/ACM (manca il file DLL?)\n"
#define MSGTR_MissingLAVCcodec "Impossibile trovare il codec '%s' in libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF mentre cercavo la sequenza di intestazione\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Impossibile leggere la sequenza di intestazione!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Impossibile leggere l'estensione della sequenza di intestazione!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Sequenza di intestazione non valida!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Estensione della sequenza di intestazione non valida!\n"

#define MSGTR_ShMemAllocFail "Impossibile allocare la memoria condivisa\n"
#define MSGTR_CantAllocAudioBuf "Impossibile allocare il buffer di uscita dell'audio\n"

#define MSGTR_UnknownAudio "Formato audio sconosciuto/mancante, non uso l'audio\n"

#define MSGTR_UsingExternalPP "[PP] Utilizzo un filtro di postprocessing esterno, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Utilizzo il postprocessing del codec, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "L'attributo video '%s' non è gestibile dal vo & vd selezionati! \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Famiglia di codec video voluta [%s] (vfm=%s) non disponibile.\nAbilitala in compilazione.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Famiglia di codec audio voluta [%s] (afm=%s) non disponibile.\nAbilitala in compilazione.\n"
#define MSGTR_OpeningVideoDecoder "Apertura decoder video: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Scelto codec video: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Apertura decoder audio: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Scelto codec audio: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Costruisco catena filtri audio per %dHz/%dch/%s -> %dHz/%dch/%s...\n"
#define MSGTR_UninitVideoStr "Uninit video: %s  \n"
#define MSGTR_UninitAudioStr "Uninit audio: %s  \n"
#define MSGTR_VDecoderInitFailed "Inizializzazione VDecoder fallita :(\n"
#define MSGTR_ADecoderInitFailed "Inizializzazione ADecoder fallita :(\n"
#define MSGTR_ADecoderPreinitFailed "Preinizializzazione ADecoder fallita :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alloco %d byte per il buffer di input\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alloco %d + %d = %d byte per il buffer di output\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurazione del supporto per LIRC...\n"
#define MSGTR_LIRCopenfailed "Apertura del supporto per LIRC fallita. Non potrai usare il tuo telecomando.\n"
#define MSGTR_LIRCcfgerr "Fallimento nella lettura del file di configurazione di LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Impossibile trovare il filtro video '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Impossibile aprire il filtro video '%s'\n"
#define MSGTR_OpeningVideoFilter "Apertura filtro video: "
#define MSGTR_CannotFindColorspace "Impossibile trovare uno spazio colore in comune, anche inserendo 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Il codec non ha impostato sh->disp_w and sh->disp_h, tento di risolvere.\n"
#define MSGTR_VoConfigRequest "VDec: configurazione chiesta dal vo - %d x %d (sp.col. preferito: %s)\n"
#define MSGTR_UsingXAsOutputCspNoY "VDec: uso %s come sp.colore di uscita (non %d)\n"
#define MSGTR_CouldNotFindColorspace "Impossibile trovare uno spazio colore adatto - riprovo con -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect è %.2f:1 - riscalo per ottenere un rapporto corretto.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect non definito - nessuna scalatura.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Devi installare o aggiornare i codec binari.\nVai su http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow inizializzato correttamente.\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO inizializzato correttamente.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Impossibile inviare l'evento schermo pieno EWMH!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: Non riesco a trovare la finestra di XScreenSaver.\n" 
#define MSGTR_SelectedVideoMode "XF86VM: Scelta modalità video %dx%d per la dimensione immagine %dx%d.\n"

#define MSGTR_InsertingAfVolume "[Mixer] Nessun mixer hardware, filtro volume inserito automaticamente.\n"
#define MSGTR_NoVolume "[Mixer] Regolazione del volume non disponibile.\n"
#define MSGTR_NoBalance "[Mixer] Controllo del bilanciamento non disponibile.\n"


// ====================== GUI messages/buttons ========================

// --- labels ---
#define MSGTR_About "Informazioni su"
#define MSGTR_FileSelect "Seleziona il file..."
#define MSGTR_SubtitleSelect "Seleziona il sottotitolo..."
#define MSGTR_OtherSelect "Seleziona..."
#define MSGTR_AudioFileSelect "Seleziona canale audio esterno..."
#define MSGTR_FontSelect "Seleziona il carattere..."
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizzatore"			 
#define MSGTR_ConfigureEqualizer "Configurazione Equalizzatore"
#define MSGTR_SkinBrowser "Gestore Skin"
#define MSGTR_Network "Flusso dati dalla rete..."
#define MSGTR_Preferences "Preferenze"
#define MSGTR_AudioPreferences "Configurazione driver audio"
#define MSGTR_NoMediaOpened "nessun media aperto"
#define MSGTR_VCDTrack "Traccia VCD %d"
#define MSGTR_NoChapter "nessun capitolo"
#define MSGTR_Chapter "capitolo %d"
#define MSGTR_NoFileLoaded "nessun file caricato"
			 
// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Annulla"
#define MSGTR_Add "Aggiungi"
#define MSGTR_Remove "Rimuovi"
#define MSGTR_Clear "Pulisci"
#define MSGTR_Config "Configura"
#define MSGTR_ConfigDriver "Configura driver"
#define MSGTR_Browse "Sfoglia"

// --- error messages ---
#define MSGTR_NEMDB "Mi dispiace, non c'è abbastanza memoria per tracciare il buffer."
#define MSGTR_NEMFMR "Mi dispiace, non c'è abbastanza memoria per visualizzare il menu."
#define MSGTR_IDFGCVD "Mi dispiace, non ho trovato un driver di output video compatibile con la GUI."
#define MSGTR_NEEDLAVC "Mi dispiace, non puoi riprodurre file non-MPEG con il tuo dispositivo DXR3/H+\nsenza ricodificarli.\nAbilita lavc nella finestra di configurazione DXR3/H+."
#define MSGTR_UNKNOWNWINDOWTYPE "Trovato tipo finestra sconosciuto..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] errore nel file di configurazione della skin alla riga %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] attenzione: nel file di configurazione della skin alla riga %d:\nwidget trovato ma non trovata prima la \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] attenzione: nel file di configurazione della skin alla riga %d:\nwidget trovato ma non trovata prima la \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] attenzione: nel file di configurazione della skin alla riga %d:\nquesta sottosezione non è supportata dal widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] file ( %s ) non trovato.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] file ( %s ) non leggibile.\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmap con profondità di 16 bit o inferiore non supportate (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "File non trovato (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP, errore di lettura (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA, errore di lettura (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG, errore di lettura (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed TGA non supportato (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "tipo di file sconosciuto (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "errore nella conversione da 24 bit a 32 bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "messaggio sconosciuto: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "memoria insufficiente\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "dichiarati troppi font\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "file dei font non trovato\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "file delle immagini dei font non trovato\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificatore del font inesistente (%s)\n"
#define MSGTR_SKIN_UnknownParameter "parametro sconosciuto  (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin non trovata (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Skin scelta ( %s ) not trovata, provo con la 'default'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Errore nella lettura del file di configurazione della skin (%s).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- GTK menus
#define MSGTR_MENU_AboutMPlayer "Informazioni su MPlayer"
#define MSGTR_MENU_Open "Apri..."
#define MSGTR_MENU_PlayFile "Riproduci il file..."
#define MSGTR_MENU_PlayVCD "Riproduci il VCD..."
#define MSGTR_MENU_PlayDVD "Riproduci il DVD..."
#define MSGTR_MENU_PlayURL "Riproduci l'URL..."
#define MSGTR_MENU_LoadSubtitle "Carica i sottotitoli..."
#define MSGTR_MENU_DropSubtitle "Elimina i sototitoli..."
#define MSGTR_MENU_LoadExternAudioFile "Carica file audio esterni..."
#define MSGTR_MENU_Playing "Riproduzione"
#define MSGTR_MENU_Play "Riproduci"
#define MSGTR_MENU_Pause "Pausa"
#define MSGTR_MENU_Stop "Interrompi"
#define MSGTR_MENU_NextStream "Flusso successivo"
#define MSGTR_MENU_PrevStream "Flusso precedente"
#define MSGTR_MENU_Size "Dimensione"
#define MSGTR_MENU_HalfSize   "Dimensione dimezzata"
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
#define MSGTR_MENU_AudioLanguages "Lingua dell'audio"
#define MSGTR_MENU_SubtitleLanguages "Lingua dei sottotitoli"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Ricerca skin"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Uscita..."
#define MSGTR_MENU_Mute "Muto"
#define MSGTR_MENU_Original "Originale"
#define MSGTR_MENU_AspectRatio "Aspetto"
#define MSGTR_MENU_AudioTrack "Traccia audio"
#define MSGTR_MENU_Track "Traccia %d"
#define MSGTR_MENU_VideoTrack "Traccia video"
#define MSGTR_MENU_Subtitles "Sottotitoli"

// --- equalizer
// Note: If you change MSGTR_EQU_Audio please see if it still fits MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "Audio"
// Note: If you change MSGTR_EQU_Video please see if it still fits MSGTR_PREFERENCES_Video
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
#define MSGTR_EQU_Channel1 "Canale 1:"
#define MSGTR_EQU_Channel2 "Canale 2:"
#define MSGTR_EQU_Channel3 "Canale 3:"
#define MSGTR_EQU_Channel4 "Canale 4:"
#define MSGTR_EQU_Channel5 "Canale 5:"
#define MSGTR_EQU_Channel6 "Canale 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Percorso"
#define MSGTR_PLAYLIST_Selected "File selezionati"
#define MSGTR_PLAYLIST_Files "File"
#define MSGTR_PLAYLIST_DirectoryTree "albero delle directory"

// --- preferences
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_Misc "Varie"
#define MSGTR_PREFERENCES_None "Nessuno"
#define MSGTR_PREFERENCES_DriverDefault "Driver predefinito"
#define MSGTR_PREFERENCES_AvailableDrivers "Driver disponibili:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Non riprodurre l'audio"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizza l'audio"
#define MSGTR_PREFERENCES_EnableEqualizer "Abilita l'equalizzatore"
#define MSGTR_PREFERENCES_SoftwareMixer "Abilita Mixer Software"
#define MSGTR_PREFERENCES_ExtraStereo "Abilita l'extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficiente:"
#define MSGTR_PREFERENCES_AudioDelay "Ritardo audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Abilita il doppio buffering"
#define MSGTR_PREFERENCES_DirectRender "Abilita il direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Abilita lo scarto dei fotogrammi"
#define MSGTR_PREFERENCES_HFrameDrop "Abilita lo scarto intenso (hard) dei fotogrammi (pericoloso)"
#define MSGTR_PREFERENCES_Flip "Ribalta l'immagine sottosopra"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timer e indicatori"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, percentuale e tempo totale"
#define MSGTR_PREFERENCES_OSDProgress "Solo indicatori"
#define MSGTR_PREFERENCES_Subtitle "Sottotitolo:"
#define MSGTR_PREFERENCES_SUB_Delay "Ritardo: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Posizione: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Disattiva il caricamento automatico dei sottotitoli"
#define MSGTR_PREFERENCES_SUB_Unicode "Sottotitoli unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Converti i sottotitoli nel formato sottotitolo di MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Converti i sottotitoli nel formato SubViewer (SRT) basato sul tempo"
#define MSGTR_PREFERENCES_SUB_Overlap "Attiva/disattiva sovrapposizione sottotitoli"
#define MSGTR_PREFERENCES_SUB_USE_ASS "Visualizzazione sottotitoli SSA/ASS"
#define MSGTR_PREFERENCES_SUB_ASS_USE_MARGINS "Usa margini"
#define MSGTR_PREFERENCES_SUB_ASS_TOP_MARGIN "Sopra: "
#define MSGTR_PREFERENCES_SUB_ASS_BOTTOM_MARGIN "Sotto: "
#define MSGTR_PREFERENCES_Font "Tipo Carattere:"
#define MSGTR_PREFERENCES_Codecs "Codec e demuxer"
#define MSGTR_PREFERENCES_FontFactor "Fattore Carattere:"
#define MSGTR_PREFERENCES_PostProcess "Abilita postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Qualità automatica: "
#define MSGTR_PREFERENCES_NI "Utilizza un analizzatore non-interleaved per i file AVI"
#define MSGTR_PREFERENCES_IDX "Ricostruisci l'indice, se necessario"
#define MSGTR_PREFERENCES_VideoCodecFamily "Famiglia codec video:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Famiglia codec audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Livello OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Sottotitoli"
#define MSGTR_PREFERENCES_FRAME_Font "Carattere"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec e demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "Dispositivo:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Canale mixer:"
#define MSGTR_PREFERENCES_Message "Ricorda che devi riavviare la riproduzione affinché alcune opzioni abbiano effetto!"
#define MSGTR_PREFERENCES_DXR3_VENC "Video encoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Usa LAVC (FFmpeg)"
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
#define MSGTR_PREFERENCES_FontEncoding22 "Slavic/Central European Windows (CP1250)"
#define MSGTR_PREFERENCES_FontEncoding23 "Arabic Windows (CP1256)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Nessun ridimensionamento automatico"
#define MSGTR_PREFERENCES_FontPropWidth "Proporzionale alla larghezza del filmato"
#define MSGTR_PREFERENCES_FontPropHeight "Proporzionale all'altezza del filmato"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporzionale alla diagonale del filmato"
#define MSGTR_PREFERENCES_FontEncoding "Codifica:"
#define MSGTR_PREFERENCES_FontBlur "Sfumatura:"
#define MSGTR_PREFERENCES_FontOutLine "Bordo:"
#define MSGTR_PREFERENCES_FontTextScale "Scala testo:"
#define MSGTR_PREFERENCES_FontOSDScale "Scala OSD:"
#define MSGTR_PREFERENCES_SubtitleOSD "Sottotitoli & OSD"
#define MSGTR_PREFERENCES_Cache "Cache on/off"
#define MSGTR_PREFERENCES_LoadFullscreen "Avvia a pieno schermo"
#define MSGTR_PREFERENCES_CacheSize "Dimensione cache: "
#define MSGTR_PREFERENCES_SaveWinPos "Salva la posizione della finestra"
#define MSGTR_PREFERENCES_XSCREENSAVER "Arresta XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Attiva playbar"
#define MSGTR_PREFERENCES_AutoSync "Autosync on/off"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "Dispositivo CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Dispositivo DVD:"
#define MSGTR_PREFERENCES_FPS "FPS del filmato:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Mostra la finestra video anche quando non è attiva"
#define MSGTR_PREFERENCES_ArtsBroken "Le nuove versioni di aRts sono incompatibili "\
           "con GTK 1.x e GMPlayer crasherà!"

#define MSGTR_ABOUT_UHU "Lo sviluppo della GUI è sponsorizzato da UHU Linux\n"
#define MSGTR_ABOUT_Contributors "Contributori codice e documentazione\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Codec e librerie di terze parti\n"
#define MSGTR_ABOUT_Translations "Traduzioni\n"
#define MSGTR_ABOUT_Skins "Skin\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Errore fatale!"
#define MSGTR_MSGBOX_LABEL_Error "Errore!"
#define MSGTR_MSGBOX_LABEL_Warning "Avvertimento!"

// bitmap.c
#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] non c'è abbastanza memoria per l'immagine\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] non c'è abbastanza memoria per l'immagine\n"

// cfg.c
#define MSGTR_ConfigFileReadError "[cfg] errore di lettura file di configurazione...\n"
#define MSGTR_UnableToSaveOption "[cfg] non riesco a salvare l'opzione '%s'.\n"

// interface.c
#define MSGTR_DeletingSubtitles "[GUI] Elimino sottotitoli.\n"
#define MSGTR_LoadingSubtitles "[GUI] Carico sottotitoli: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Aggiungo filtro video: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Rimuovo filtro video: %s\n"

// mw.c
#define MSGTR_NotAFile "Questo non pare essere un file: %s !\n"

// ws.c
#define MSGTR_WS_CouldNotOpenDisplay "[ws] Non posso aprire il display.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Display remoto, disabilito XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Spiacente, il tuo sistema non supporta l'estensione 'X shared memory'.\n"
#define MSGTR_WS_NoXshape "[ws] Spiacente, il tuo sistema non supporta l'estensione XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Spiacente, la profondità colore è troppo bassa.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Ci sono troppe finestre aperte.\n"
#define MSGTR_WS_ShmError "[ws] errore estensione 'shared memory'\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Spiacente, non abbastanza memoria per il tracciare il buffer.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS non disponibile?\n"
#define MSGTR_WS_DpmsNotEnabled "Non posso abilitare DPMS.\n"

// wsxdnd.c
#define MSGTR_WS_NotAFile "Questo non sembra essere un file...\n"
#define MSGTR_WS_DDNothing "D&D: Nessun valore di ritorno!\n"


// ======================= video output drivers ========================

#define MSGTR_VOincompCodec "Il dispositivo di uscita video_out scelto è incompatibile con questo codec.\n"\
                "Prova aggiungendo il filtro scale alla sequenza dei filtri,\n"\
                "per esempio -vf spp,scale invece di -vf spp.\n"
#define MSGTR_VO_GenericError "E' accaduto questo errore"
#define MSGTR_VO_UnableToAccess "Impossibile accedere a"
#define MSGTR_VO_ExistsButNoDirectory "già esiste, ma non è una directory."
#define MSGTR_VO_DirExistsButNotWritable "La directory di output esiste già ma non è scrivibile."
#define MSGTR_VO_DirExistsAndIsWritable "La directory di output esiste già ed è scrivibile."
#define MSGTR_VO_CantCreateDirectory "Non posso creare la directory di output."
#define MSGTR_VO_CantCreateFile "Non posso creare il file di output."
#define MSGTR_VO_DirectoryCreateSuccess "Directory di output creata con successo."
#define MSGTR_VO_ParsingSuboptions "Leggo sottoopzioni."
#define MSGTR_VO_SuboptionsParsedOK "Lettura sottoopzioni OK."
#define MSGTR_VO_ValueOutOfRange "Valore fuori gamma"
#define MSGTR_VO_NoValueSpecified "Nessun valore specificato."
#define MSGTR_VO_UnknownSuboptions "Sottoopzione/i sconosciuta/e"

// vo_aa.c
#define MSGTR_VO_AA_HelpHeader "\n\nEcco le sottoopzioni per l'aalib vo_aa:\n"
#define MSGTR_VO_AA_AdditionalOptions "Le opzioni addizionali di vo_aa:\n" \
"  help        mostra questo messaggio\n" \
"  osdcolor    imposta colore OSD\n" \
"  subcolor    imposta colore sottotitoli\n" \
"        i colori possibili sono:\n"\
"           0 : normal\n" \
"           1 : dim\n" \
"           2 : bold\n" \
"           3 : boldfont\n" \
"           4 : reverse\n" \
"           5 : special\n\n\n"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Progressive JPEG abilitata."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Progressive JPEG disabilitata."
#define MSGTR_VO_JPEG_BaselineJPEG "Baseline JPEG abilitata."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline JPEG disabilitata."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Modalità ASCII abilitata."
#define MSGTR_VO_PNM_RawMode "Modalità Raw abilitata."
#define MSGTR_VO_PNM_PPMType "Scriverò files PPM."
#define MSGTR_VO_PNM_PGMType "Scriverò files PGM."
#define MSGTR_VO_PNM_PGMYUVType "Scriverò files PGMYUV."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "La modalità interlacciata richiede l'altezza immagine divisibile per 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Impossibile allocare il buffer di linea per la modalità interlacciata." 
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "L'input non è RGB, non posso separare la crominanza per campi!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "La larghezza immagine dev'essere divisibile per 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Non c'è abbastanza memoria per allocare il framebuffer RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Non posso allocare memoria o spazio per scrivere \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Errore di scrittura dell'immagine in uscita!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Subdispositivo sconosciuto: %s"
// 'top-field first'/'bottom-field first' should be left as they are written,
// since there's a reference to these in the man page
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Uso modalità di uscita interlacciata, top-field first."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Uso modalità di uscita interlacciata, bottom-field first."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Uso la modalità fotogramma progressivo (default)."

// sub.c
#define MSGTR_VO_SUB_Seekbar "Barra ricerca"
#define MSGTR_VO_SUB_Play "Riproduci"
#define MSGTR_VO_SUB_Pause "Pausa"
#define MSGTR_VO_SUB_Stop "Stop"
#define MSGTR_VO_SUB_Rewind "Indietro"
#define MSGTR_VO_SUB_Forward "Avanti"
#define MSGTR_VO_SUB_Clock "Orologio"
#define MSGTR_VO_SUB_Contrast "Contrasto"
#define MSGTR_VO_SUB_Saturation "Saturazione"
#define MSGTR_VO_SUB_Volume "Volume"
#define MSGTR_VO_SUB_Brightness "Luminosità"
#define MSGTR_VO_SUB_Hue "Tonalità"
#define MSGTR_VO_SUB_Balance "Bilanciamento"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "Dimensioni dell'immagine origine troppo grandi: %ux%u (il massimo è %ux%u)\n"

// old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Il driver di uscita video pgm è stato sostituito con -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Il driver di uscita video md5 è stato sostituito con -vo md5sum.\n"


// ======================= audio output drivers ========================

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: i moduli alsa9/alsa1x sono stati rimossi, ora usa -ao alsa.\n"
#define MSGTR_AO_TryingPreferredAudioDriver "Provo con il driver audio preferito '%.*s', opzioni '%s'\n"
#define MSGTR_AO_NoSuchDriver "driver audio '%.*s' non trovato\n"
#define MSGTR_AO_FailedInit "Inizializzazione del driver audio '%s' non riuscita\n"
#define MSGTR_AO_TryingEveryKnown "Provo con tutti i driver audio conosciuti...\n"


// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Non posso aprire il dispositivo mixer %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Il mixer scheda audio non ha il canale '%s', uso default.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Non posso aprire il dispositivo audio %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Non riesco a bloccare il dispositivo: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Non posso impostare il device audio %s a %s output, provo %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Fallita impostazione audio a %d canali.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: il driver non supporta SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Il tuo driver audio NON supporta select()  ***\n Ricompila MPlayer con #undef HAVE_AUDIO_SELECT in config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nErrore fatale: *** NON POSSO RIAPRIRE / RESETTARE IL DEVICE AUDIO *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] Formato OSS sconosciuto/non supportato: %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Connesso al server del suono.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Non posso apire un flusso.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Flusso aperto.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] dimensione buffer: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Impostazione del volume a %d fallita.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz non supportati, prova a ricampionare (resample).\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound fallito: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latenza: [server: %0.2fs, net: %0.2fs] (scarto %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] fallimento nell'aprire il flusso di riproduzione ESD: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Impostazione mixer DVB fallita: %s.\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz non supportati, prova a ricampionare (resample).\n"

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] File: %s (%s)\nPCM: Samplerate: %iHz Canali: %s Formato %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: Un dump più veloce si ottiene con -vc null -vo null -ao pcm:fast\n[AO PCM] Info: Per scrivere file WAVE usa -ao pcm:waveheader (default).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Non posso aprire %s in scrittura!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Canali: %s Formato %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] Uso il driver audio %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Formato audio non supportato: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Inizializzazione audio SDL fallita: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Non posso aprire l'audio: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] control.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Canali: %s Formato %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: dispositivo non valido.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams fallito: %s\nNon posso impostare il samplerate voluto.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE non è stato accettato dalla risorsa.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams fallito: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: il samplerate ora è %lf (la frequenza voluta è %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Non posso apire il canale audio: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO fallito.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: scrittura fallita."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Non posso aprire il dispositivo audio %s, %s  -> no audio.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: la tua scheda non supporta il canale %d, %s, %d Hz samplerate.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Il tuo driver audio NON supporta select()  ***\n Ricompila MPlayer con #undef HAVE_AUDIO_SELECT in config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nErrore fatale: *** NON POSSO RIAPRIRE / RESETTARE IL DEVICE AUDIO *** %s\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: formato richiesto: %d Hz, %d canali, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: nessuna scheda audio trovata.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: formato voluto (%s) invalido - output disabilitato.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: errore apertura riproduzione: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: errore informazioni PCM: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d scheda/e audio travata/e, uso: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: errore informazioni canale PCM: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: errore impostazione parametri: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: errore nell'impostazione canale: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: preparazione del canale: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: errore 'drain' riproduzione: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: errore 'flush' riproduzione: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: errore chiusura PCM: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: errore 'drain' riproduzione: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: errore 'flush' riproduzione: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: errore preparazione canale: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: errore 'drain' riproduzione: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: errore 'flush' riproduzione: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: errore preparazione canale: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: ritardo alsa, reimposto il flusso.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: errore preparazione riproduzione: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: errore di scrittura dopo reset: %s - mi arrendo.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: errore di output: %s\n"

// ao_alsa.c
#define MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero "[AO_ALSA] Indice mixer non valido. Uso il default 0.\n"
#define MSGTR_AO_ALSA_MixerOpenError "[AO_ALSA] Errore di apertura mixer: %s\n"
#define MSGTR_AO_ALSA_MixerAttachError "[AO_ALSA] Errore di collegamento al mixer %s: %s\n"
#define MSGTR_AO_ALSA_MixerRegisterError "[AO_ALSA] Errore registrazione mixer: %s\n"
#define MSGTR_AO_ALSA_MixerLoadError "[AO_ALSA] Errore caricamento mixer: %s\n"
#define MSGTR_AO_ALSA_UnableToFindSimpleControl "[AO_ALSA] Impossibile trovare il controllo semplice '%s',%i.\n"
#define MSGTR_AO_ALSA_ErrorSettingLeftChannel "[AO_ALSA] Errore impostando il canale sinistro, %s\n"
#define MSGTR_AO_ALSA_ErrorSettingRightChannel "[AO_ALSA] Errore impostando il canale destro, %s\n"
#define MSGTR_AO_ALSA_CommandlineHelp "\n[AO_ALSA] riga comando per -ao alsa:\n"\
"[AO_ALSA] Esempio: mplayer -ao alsa:device=hw=0.3\n"\
"[AO_ALSA]   Seleziona la prima carta quarto dispositivo hardware.\n\n"\
"[AO_ALSA] Opzioni:\n"\
"[AO_ALSA]   noblock\n"\
"[AO_ALSA]     Apre il dispositivo in modalita' non bloccante.\n"\
"[AO_ALSA]   device=<nome-dispositivo>\n"\
"[AO_ALSA]     Seleziona il dispositivo (cambia i , in . e i : in =)\n"
#define MSGTR_AO_ALSA_ChannelsNotSupported "[AO_ALSA] non sono supportati %d canali.\n"
#define MSGTR_AO_ALSA_OpenInNonblockModeFailed "[AO_ALSA] Apertura in modalità non bloccante fallita, provo con la bloccante.\n"
#define MSGTR_AO_ALSA_PlaybackOpenError "[AO_ALSA] Errore aprendo il Playback: %s\n"
#define MSGTR_AO_ALSA_ErrorSetBlockMode "[AL_ALSA] Errore impostando la modalità bloccante %s.\n"
#define MSGTR_AO_ALSA_UnableToGetInitialParameters "[AO_ALSA] Impossibile impostare i paramtri iniziali: %s\n"
#define MSGTR_AO_ALSA_UnableToSetAccessType "[AO_ALSA] Impossibile impostare il tipo di accesso: %s\n"
#define MSGTR_AO_ALSA_FormatNotSupportedByHardware "[AO_ALSA] Il formato %s non è supportato dall'hardware, provo col default.\n"
#define MSGTR_AO_ALSA_UnableToSetFormat "[AO_ALSA] Impossibile impostare il formato: %s\n"
#define MSGTR_AO_ALSA_UnableToSetChannels "[AO_ALSA] Impossibile impostare i canali: %s\n"
#define MSGTR_AO_ALSA_UnableToDisableResampling "[AO_ALSA] Impossibile disabilitare il ricampionamento: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSamplerate2 "[AO_ALSA] Impossibile impostare samplerate-2: %s\n"
// TODO::
//#define MSGTR_AO_ALSA_UnableToSetBufferTimeNear "[AO_ALSA] Unable to set buffer time near: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodTime "[AO_ALSA] Impossibile impostare il tempo del periodo: %s\n"
#define MSGTR_AO_ALSA_UnableToGetPeriodSize "[AO ALSA] Impossibile ricavare la dimensione periodo: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodSize "[AO ALSA] Impossibile impostare la dimensione periodo(%ld): %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriods "[AO_ALSA] Impossibile impostare i periodi: %s\n"
#define MSGTR_AO_ALSA_UnableToSetHwParameters "[AO_ALSA] Impossibile impostare hw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBufferSize "[AO_ALSA] Impossibile ricavare buffersize: %s\n"
#define MSGTR_AO_ALSA_UnableToGetSwParameters "[AO_ALSA] Impossibile ricavare sw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSwParameters "[AO_ALSA] Impossibile impostare sw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBoundary "[AO_ALSA] Impossibile ricavare boundary: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStartThreshold "[AO_ALSA] Impossibile impostare soglia d'inizio: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStopThreshold "[AO_ALSA] Impossibile impostare soglia di termine: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSilenceSize "[AO_ALSA] Impossibile impostare dimensione del silenzio: %s\n"
#define MSGTR_AO_ALSA_PcmCloseError "[AO_ALSA] errore chiusura pcm: %s\n"
#define MSGTR_AO_ALSA_NoHandlerDefined "[AO_ALSA] Nessun gestore definito!\n"
#define MSGTR_AO_ALSA_PcmPrepareError "[AO_ALSA] errore preparazione pcm: %s\n"
#define MSGTR_AO_ALSA_PcmPauseError "[AO_ALSA] errore pausa pcm: %s\n"
#define MSGTR_AO_ALSA_PcmDropError "[AO_ALSA] errore scarto pcm: %s\n"
#define MSGTR_AO_ALSA_PcmResumeError "[AO_ALSA] errore ripresa pcm: %s\n"
#define MSGTR_AO_ALSA_DeviceConfigurationError "[AO_ALSA] Errore configurazione dispositivo."
#define MSGTR_AO_ALSA_PcmInSuspendModeTryingResume "[AO_ALSA] Pcm in modalità sospesa, provo a riprendere.\n"
#define MSGTR_AO_ALSA_WriteError "[AO_ALSA] Errore in scrittura: %s\n"
#define MSGTR_AO_ALSA_TryingToResetSoundcard "[AO_ALSA] Provo a reimpostare la scheda audio.\n"
#define MSGTR_AO_ALSA_CannotGetPcmStatus "[AO_ALSA] Impossibile ricavare lo stato pcm: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] plugin non valido: %s\n"


// ======================= audio filters ================================

// libaf 

// af_ladspa.c
#define MSGTR_AF_LADSPA_AvailableLabels "etichette disponibili in"
#define MSGTR_AF_LADSPA_WarnNoInputs "WARNING! Questo plugin LADSPA non ha entrate audio.\n  Il segnale audio in entrata verrà perso."
#define MSGTR_AF_LADSPA_ErrMultiChannel "I plugin multi-canale (>2) non sono supportati (finora).\n  Usare solo i plugin mono e stereo."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Questo plugin LADSPA non ha uscite audio."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Il numero delle entrate e uscite audio del plugin LADSPA differiscono."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "fallimento nel caricare"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Non trovo la funzione ladspa_descriptor() nella libreria specificata."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Non trovo l'etichetta nella libreria plugin."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Nessuna sottoopzione specificata."
#define MSGTR_AF_LADSPA_ErrNoLibFile "Nessuna libreria specificata."
#define MSGTR_AF_LADSPA_ErrNoLabel "Nessuna etichetta di filtro specificata."
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Non sono stati specificati abbastanza controlli sulla riga comando."
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Il controllo di input #%d è sotto il limite inferiore di %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Il controllo di input #%d è sopra al limite superiore di %0.4f.\n"

// format.c
#define MSGTR_AF_FORMAT_UnknownFormat "formato sconosciuto "


// ========================== INPUT =========================================

// joystick.c
#define MSGTR_INPUT_JOYSTICK_Opening "Apertura dispositivo joystick %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Non posso aprire il dispositivo joystick %s: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Errore in lettura dispositivo joystick: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Joystick: Persi %d byte di dati\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Joystick: Evento di attenzione in inizializzazione, persa sincronia col driver.\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Joystick: Avviso: tipo di evento %d sconosciuto\n"

// appleir.c
#define MSGTR_INPUT_APPLE_IR_Init "Inizializzo l'Apple IR su %s\n"
#define MSGTR_INPUT_APPLE_IR_Detect "Rilevato Apple IR su %s\n"
#define MSGTR_INPUT_APPLE_IR_CantOpen "Impossibile aprire il dispositivo Apple IR: %s\n"

// input.c
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Troppi descrittori comandi, non posso registrare il descrittore %d.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Troppi descrittori tasti, non posso registrare il descrittore %d.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "Comando %s: l'argomento %d non è un intero.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "Comando %s: l'argomento %d non èun decimale.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "Comando %s: l'argomento %d è indeterminato.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Argomento %d sconosciuto\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "Il comando %s richiede almeno %d argomenti, trovati solo %d finora.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Errore in lettura descrittore comandi %d: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "Il buffer cmd del descrittore %d è pieno: ne scarto il contenuto.\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Controllo non valido legato al tasto '%s'"
#define MSGTR_INPUT_INPUT_ErrSelect "Errore selezione: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Errore nel descrittore input tasto %d\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Input da tasto morto nel descrittore %d\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Troppi eventi di 'tasto premuto' contemporaneamente\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Errore nel descrittore comando %d\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Fallimento nella lettura del file di configurazione dell'input %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Il tasto '%s' è sconosciuto\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Legame %s non completato\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "Il buffer è troppo piccolo per questo nome tasto: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "Nessun comando trovato per il tasto %s"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "Il buffer è troppo piccolo per il comando %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "Cosa ci stiamo facendo qui?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "Impossibile inizializzare i controlli del joystick\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "Impossibile fare stat di %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Impossibile aprire %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantInitAppleRemote "Impossibile inizializzare l'Apple Remote.\n"


// ========================== LIBMPDEMUX ===================================

// url.c
#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "La stringa sembra essere già filtrata in url_escape %c%c1%c2\n"

// ai_alsa1x.c
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Non posso impostare il samplerate.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Non posso impostare il tempo del buffer.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Non posso impostare il tempo del periodo.\n"

// ai_alsa1x.c / ai_alsa.c
#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Configurazione PCM rovinata: nessuna configurazione disponibile.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Tipo di accesso non disponibile.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Formato sample non disponibile.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Calcolo canali non disponibile - reimposto al default: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Impossibile impostare i parametri hardware: %s\n"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Non posso usare il periodo uguale al buffer (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Impossibile impostare i parametri software:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Non posso aprire l'audio: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "ALSA Errore di stato: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (almeno di %.3f ms)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "ALSA Stato:\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: errore preparazione: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA errore di lettura/scrittura"

// ai_oss.c
#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Impossibile impostare il numero di canali: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Impossibile impostare lo stereo: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "Non posso aprire '%s': %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "Formato non supportato\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Impossibile impostare il formato audio."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "Impossibile impostare il samplerate: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "Impossibile impostare il trigger: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Non posso ricavare la dimensione del blocco!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "La dimensione del blocco audio è zero, la imposto a %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "La dimensione del blocco audio è troppo bassa, la imposto a %d!\n"

// asfheader.c
#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "FATALE: la dimensione dell'intestazione è più grande di 1 MB (%d)!\nPerfavore contatta gli autori di MPlayer e invia/rendi disponibile questo file.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Non ho potuto allocare %d byte per l'intestazione.\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "EOF durante la lettura dell'intestazione ASF, file danneggiato/incompleto?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR funzionerà probabilmente solo con libavformat, prova -demuxer 35 se hai problemi\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Nessun dato trovato dopo l'intestazione!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: nessuna intestazione audio o video trovata - file danneggiato?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Lunghezza non valida nell'intestazione ASF!\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMLicenseURL "URL Licenza DRM: %s\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMProtected "Questo file è stato oscurato con protezione DRM, non potrà esser riprodotto in MPlayer!\n"

// asf_mmst_streaming.c
#define MSGTR_MPDEMUX_MMST_WriteError "Errore di scrittura\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nAttenzione! EOF\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "lettura pre-intestazione fallita.\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Dimensione intestazione non valida, mi arrendo.\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "lettura dati intestazione fallita.\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "lettura packet_len fallita.\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Dimensione pacchetto rtsp invalida, mi arrendo.\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "lettura dati comando fallita.\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "oggetto intestazione\n"
#define MSGTR_MPDEMUX_MMST_DataObject "oggetto dati\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "oggetto file, lunghezza pacchetto (packet length) = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "oggetto flusso, id flusso: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "troppi id, flusso scartato"
#define MSGTR_MPDEMUX_MMST_UnknownObject "oggetto sconosciuto\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Lettura dati contenuto fallita.\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "firma mancante\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Tutto fatto. Grazie per aver scaricato un file che contiene tecnologie proprietarie e brevettate.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "comando sconosciuto %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "errore get_media_packet: %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "connesso\n"

// asf_streaming.c
#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, la dimensione stream_chunk è troppo piccola: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "disallineamento size_confirm!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Attenzione: scarto intestazione ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Errore durante l'interpretazione di una parte di intestazione\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Non ho avuto un'intestazione come prima parte!!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Errore non posso allocare un buffer di %d byte.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Errore durante la lettura del flusso via rete.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Errore chunk troppo piccolo.\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Errore il numero delle sotto-parti non è valido.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "banda troppo piccola, il file non può esser riprodotto!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "banda troppo piccola, deselezionato flusso audio.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "banda troppo piccola, deselezionato flusso video.\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Lunghezza non valida nell'intestazione ASF!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Errore durante la lettura di una parte di intestazione.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Errore chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Errore durante la lettura di una parte (chunk).\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Ridirezionatore\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "URL proxy non valido\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "Tipo del flusso asf sconosciuto\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Interpretazione della risposta HTTP fallita.\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Il server risponde %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "AVVISO INTERPRETAZIONE ASF HTTP : Pragma %s tagliato da %d byte a %d\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Errore scrittura socket: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Interpretazione intestazione fallita.\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Nessun flusso trovato.\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "Modo do fornire il flusso ASF sconociuto\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "fallimento, esco.\n"

// audio_in.c
#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nerrore leggendo l'audio: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Recupero da un cross-run, alcuni fotogrammi possono essere stati persi!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Errore fatale, non posso recuperare!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nnon ci sono abbastanza parti audio!\n"

// aviheader.c
#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** lista vuota?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Trovato filmato a 0x%X - 0x%X\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "trovato 'bih', %u byte di %d\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "Ricostruisco tabella keyframe per video M$ mpg4v1.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Ricostruisco tabella keyframe per video DIVX3.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Ricostruisco tabella keyframe per video MPEG4.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "trovato 'wf', %d byte di %d\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: dmlh trovato (dimensione=%d) (fotogrammi totali=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "Leggo blocco INDEX, %d parti per %d fotogrammi (fpos=%"PRId64").\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "intestazione RIFF supplementare...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** Attenzione: questa non è un'intestazione AVI estesa..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Blocco (chunk) danneggiato?  chunksize=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: Costruisco indice ODML (%d superindexchunks).\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Rilevato file rovinato (incompleto?). SarÃÂ  usato un indice tradizionale.\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "Impossibile leggere il file indice %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s non è un file indice di MPlayer valido.\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Impossibile allocare la memoria per i dati dell'indice da %s.\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "fine prematura del file indice %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Caricato file indice: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Genero indice: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Tabella indice generata per %d parti (chunks)!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Non ho potuto scrivere il file indice %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Salvato file indice: %s\n"

// cache2.c
#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rNon posso riempire la cache per questo flusso.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos è differente!!! Riporta questo errore...\n"

// cdda.c
#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Impossibile aprire il dispositivo CDDA.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Impossibile aprire il disco.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "Trovato CD audio con %ld tracce.\n"

// cddb.c
#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Imposibile leggere la TOC.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Apertura del dispositivo %s fallita.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "URL non valido\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "Fallimento nell'invio della richiesta HTTP.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "Fallimento nella lettura della risposta HTTP.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Non Trovato.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "Codice di Errore sconosciuto\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Cache non trovata.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Non tutti i file xmcd sono stati letti.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Creazione della directory %s fallita.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Non tutti i file xmcd sono stati scritti.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "Ricevuto file database xmcd non valido.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "'FIXME' inatteso\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "codice non gestito\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Impossibile trovare il carattere di 'fine linea'.\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Lettura OK, trovato: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album non trovato.\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Il server ha risposto: Errore sintassi comando ('Command syntax error')\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Nessuna informazione disponibile sul sito.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Fallimento nell'ottenere il livello del protocollo.\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "Nessun CD nel lettore.\n"

// cue_read.c
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Linea del file cue inattesa: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] Verificato nome del file bin: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Impossibile trovare il file bin - mi arrendo.\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] Uso il file bin %s\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] Modalità del file bin sconosciuta. Non dovrebbe capitare. Annullamento.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] Impossibile aprire %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Errore in lettura da %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Errore nell'ottenere la dimensione del file bin.\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "traccia %02d:  formato=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] Fine inaspettata del file bin\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Non ho potuto leggere %d byte di payload.\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, nomefile=%s, traccia=%d, tracce disponibili: %d -> %d\n"

// network.c
#define MSGTR_MPDEMUX_NW_UnknownAF "Famiglia d'indirizzi %d sconosciuta\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "Risolvo %s per %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Impossibile risolvere il nome per %s: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Connessione al server %s[%s]: %d ...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Connessione al server fallita con %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Selezione fallita.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "Timeout connessione.\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "Fallimento di getsockopt: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "Errore di connessione: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Impostazioni proxy non valide... Provo senza proxy.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Non ho potuto risolvere il nome host remoto per AF_INET. Provo senza proxy.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Errore nell'invio della richiesta HTTP: Non è stata inviata tutta la richiesta.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Lettura fallita.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response ha letto uno 0 (per esempio EOF).\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Autenticazione fallita. Per favore usa le opzioni -user e -passwd per fornire\nun nome utente/password per una lista di URL, o indica un URL come questo:\nhttp://utente:password@nomehost/file\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Autenticazione richiesta per %s\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Autenticazione richiesta.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Nessuna password fornita, provo senza.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Risposta del server %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Dimensione cache impostata a %d KByte\n"

// demux_audio.c
#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "Demuxer audio: formato %d sconosciuto.\n"

// demux_demuxers.c
#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "Errore in fill_buffer: cattivo demuxer: non vd, ad o sd.\n"

// demux_mkv.c
#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] inizializzazione zlib fallita.\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] decompressione zlib fallita.\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] inizializzazione lzo fallita.\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] decompressione lzo fallita.\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] La traccia numero %u è criptata e la decodifica non è stata ancora\n[mkv] implementata. Ignoro la traccia.\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] Tipo di codifica sconosciuto per la traccia %u. Ignoro la traccia.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] La traccia %u è stata compressa con un algoritmo di compressione (%u)\nsconosciuto/non supportato. Ignoro la traccia.\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] La traccia %u è stata compressa con zlib ma mplayer non è stato compilato\n[mkv] con il supporto per tale compressione. Ignoro la traccia.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] Codec ID '%s' sconosciuto/non supportato per la traccia %u o dati\n[mkv] interni del codec mancanti/errati.\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] La traccia FLAC non contiene intestazioni valide.\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv] Codec audio ID '%s' sconosciuto/non supportato per la traccia %u o dati\n[mkv] interni del codec mancanti/errati.\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] Il tipo di sottotitoli '%s' non è gestito.\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] Riprodurrò la traccia video %u.\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] Nessuna traccia video trovata/voluta.\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] Nessuna traccia audio trovata/voluta.\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] Mostrerò la traccia sottotitoli %u.\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] Attenzione: Non trovata BlockDuration per la traccia sottotitoli.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Attenzione: troppe sottolinee da visualizzare, le salto.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] Attenzione: troppe sottolinee da visualizzare, le salto dopo le prime %i.\n"

// demux_nuv.c
#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "Nessun blocco video nel file.\n"

// demux_xmms.c
#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Trovato plugin: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Chiudo plugin: %s.\n"
#define MSGTR_MPDEMUX_XMMS_WaitForStart "Attendo che il plugin XMMS inizi la riproduzione di '%s'...\n"


// ========================== LIBMENU ===================================

// common
#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] Nessuna voce trovata nella definizione del menu.\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] errore di sintassi alla riga: %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] per le definizioni di Menu serve un nome (riga %d).\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] attributo %s=%s invalido nel menu '%s' alla riga %d\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] tipo di menu '%s' invalido alla riga %d\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] Impossibile aprire il file di configurazione menu: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] Il file di configurazione è troppo grande (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] Il file di configurazione è vuoto.\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] Menu %s non trovato.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] Menu '%s': Inizializzazione fallita.\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] Formato di uscita non supportato!!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] per le definizioni dei Menu Lista serve un nome (riga %d).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] per i Menu Lista serve un argomento.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Errore di attesa pid: %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Errore di selezione.\n"
#define MSGTR_LIBMENU_ReadErrorOnChildFD "[MENU] Errore di lettura sul descrittore file del processo figlio: %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] Esecuzione in console: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] Un processo figlio è già in esecuzione.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Fork fallito !!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] errore di scrittura\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] Errore in opendir: %s\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] Errore in realloc: %s\n"
#define MSGTR_LIBMENU_MallocError "[MENU] Errore in allocazione di memoria: %s\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] Errore in readdir: %s\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] Impossibile aprire la directory %s.\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] per le definizioni di Submenu serve un attributo 'menu'.\n"
#define MSGTR_LIBMENU_InvalidProperty "[MENU] Proprietà '%s' non valida nella definizione di Menu Pref (riga %d).\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] per le definizioni di Menu Pref serve un attributo 'property' o 'txt' valido (riga %d).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] per i Menu Pref serve un argomento.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] Impossibile trovare l'oggetto di destinazione ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] Fallimento nella creazione comando: %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] per i Menu Text serve il nome di un file di testo (parametro file).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] Impossibile aprire %s.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] Attenzione, riga troppo lunga. La spezzo.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] Lette %d righe.\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] Comando sconosciuto: '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] Impossibile aprire il menu: '%s'.\n"


// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] Attenzione! Dimensione frame audio diversa! letta=%d  hdr=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Impossibile allocare l'immagine per il codec cinepak.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] Codec XVMC accellerato.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] Significato aritmetico di QP: %2.4f, significato armonico di QP: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] Errore DRI.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Impossibile allocare l'immagine per il codec.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC accellerato MPEG-2.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] Provo pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] mc_get_buffer dovrebbe funzionare solo con accelerazione XVMC!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Errore inatteso in init_vo.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Errore irrecuperabile, buffer di renderizzazione non occupati.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Sono permessi solo i buffer allocati da vo_xvmc.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] Scelta codifica ad alta qualità (non-realtime)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] Uso qscale costante = %f (VBR).\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] Output grezzo con FourCC [%x] non supportato!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] Il codec VfW richiesto non è specificato!!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] Posizione/larghezza/altezza invalide - area ritagliata al di fuori dell'originaria!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] Area ritaglio: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] Nome formato sconosciuto: '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] Errorie interpretando l'argumento.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "Tipo compressore: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "Sottotipo compressore: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "Flag del compressore: %lu, versione %lu, versione ICM: %lu\n"
#define MSGTR_MPCODECS_Flags "Flag:"
#define MSGTR_MPCODECS_Quality " qualità"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "DR completo non possibile, provo invece gli SLICES!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "ATTENZIONE! Il filtro successivo non supporta gli SLICES, aspettati un sig11...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "Perché abbiamo avuto un NULL??\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "%s non supportato dal filtro/vo successivo :(\n"


// ================================== LIBVO ====================================

// mga_common.c
#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] errore in ioctl di mga_vid_config (versione sbagliata di mga_vid.o?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] Impossibile ottenere i valori di luminanza dal modulo del kernel!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] Impossibile impostare i valori di luminanza dal modulo del kernel!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] Larghezza/altezza schermo sconosciute!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] formato di uscita %0X invalido\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] La versione del tuo driver mga_vid è incompatibile con questo MPlayer!\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] Impossibile aprire: %s\n"
#define MSGTR_LIBVO_MGA_ResolutionTooHigh "[MGA] La risoluzione del sorgente ha almeno una delle due dimensioni maggiore di 1023x1023. Per favore ridimensiona via software o usa -lavdopts lowres=1\n"
#define MSGTR_LIBVO_MGA_mgavidVersionMismatch "[MGA] incongruenza tra le versioni del driver mga_vid del kernel (%u) e di MPlayer (%u)\n"

// libvo/vesa_lvo.c
#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] Questo branch non è più supportato.\n[VESA_LVO] Per favore usa al suo posto -vo vesa:vidix.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] Impossibile aprire: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] formato di uscita non valido: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] La versione del tuo driver fb_vid è incompatibile con questo MPlayer!\n"

// libvo/vo_3dfx.c
#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] Supportati solo 16bpp!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] L'ID visuale è %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] Impossibile aprire /dev/3dfx.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] Errore: %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] Impossibile mappare le aree di memoria 3dfx: %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] Initializzato: %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] sottodispositivo sconosciuto: %s.\n"

// libvo/aspect.c
#define MSGTR_LIBVO_ASPECT_NoSuitableNewResFound "[ASPECT] Attenzione: Non ho trovato alcuna nuova risoluzione accettabile!\n"
#define MSGTR_LIBVO_ASPECT_NoNewSizeFoundThatFitsIntoRes "[ASPECT] Errore: Non ho trovato una nuova dimensione inferiore alla risoluz.!\n"

// libvo/vo_dxr3.c
#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] Impossibile caricare una nuova palette SPU!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] Impossibile impostare la modalità riproduzione!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] Impossibile impostare la modalità subpicture!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] Impossibile ricavare la norma TV!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] Scelta automaticamente la norma TV dalla frequenza: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] Impossibile impostare la norma TV!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] Imposto per NTSC.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] Imposto per PAL/SECAM.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] Imposto il rapporto di aspetto a 4:3.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] Imposto il rapporto di aspetto a 16:9.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] memoria esaurita\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] Impossibile allocare keycolor!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] Impossibile allocare keycolor esatto, uso il più vicino (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] De-inizializzo.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] Ripristino norma TV fallito!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] Abilito il prebuffering.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] Uso il nuovo motore di sincronizzazione.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] Uso l'overlay.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] Errore: Overlay richiede la compilazione con gli header/librerie X11 installati.\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] Imposterò la norma TV a: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "imposto automaticamente alla frequenza filmato (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "imposto automaticamente alla frequenza filmato (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "Uso la norma attuale."
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "Norma richiesta sconosciuta. Uso la norma attuale."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] Errore aprendo %s in scrittura, provo con /dev/em8300.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] Errore aprendo %s in scrittura, provo con /dev/em8300_mv.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] Errore anche aprendo /dev/em8300 in scrittura!\nAbbandono.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] Errore anche aprendo /dev/em8300_mv in scrittura!\nAbbandono.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] Aperto: %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] Errore aprendo %s in scrittura, provo con /dev/em8300_sp.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] Errore anche aprendo /dev/em8300_sp in scrittura!\nAbbandono.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] Impossibile aprire il display nell'hack di impostazione overlay!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] Impossibile inizializzare X11!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] Impostazione attributo overlay fallita.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] Impostazione schermo overlay fallita!\nEsco.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] Impostazione overlay fallita!\nEsco.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] Ridimensionamento finestra overlay fallita!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] Impostazione bcs overlay fallita!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] Impossibile ricavare i valori di overlay Y-offset!\nEsco.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] Impossibile ricavare i valori di overlay Y-offset!\nEsco.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] Impossibile ricavare la correzione overlay della dimensione X!\nEsco.\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] Impossibile impostare il mix segnale!\n"

// libvo/font_load_ft.c
#define MSGTR_LIBVO_FONT_LOAD_FT_NewFaceFailed "New_Face in errore. Forse il percorso del font è errato.\nPer favore indica il file dei font per il testo (~/.mplayer/subfont.ttf).\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_NewMemoryFaceFailed "New_Memory_Face in errore..\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_SubFaceFailed "font sottotitoli: load_sub_face in errore.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_SubFontCharsetFailed "font sottotitoli: prepare_charset in errore.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareSubtitleFont "Impossibile preparare il font dei sottotitoli.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareOSDFont "Impossibile preparare il font per l'OSD.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotGenerateTables "Impossibile generare le tabelle.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_DoneFreeTypeFailed "FT_Done_FreeType in errore.\n"

// libvo/vo_mga.c
#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): ridimensionato a %dx%d.\n"

// libvo/vo_null.c
#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] Sottodispositivo sconosciuto: %s.\n"

// libvo/vo_png.c
#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] Attenzine: livello compressione a 0, compressione disabilitata!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] Info: Usa -vo png:z=<n> per impostare il livello compressione da 0 a 9.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] Info: (0 = nessuna, 1 = più veloce e peggiore - 9 migliore e più lenta)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] Errore aprendo '%s' in scrittura!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] Errore in create_png.\n"

// libvo/vo_sdl.c
#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] Impossibile ottenere una modalità SDL accettabile per l'output.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode fallito: %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode fallito: %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] Mappo I420 su IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] Formato immagine non supportato (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] Info - per favore usa -vm or -zoom per passare alla risoluzione migliore.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] Impossibile impostare la modalità video: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] Impossibile creare un overlay YUV: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] Impossibile creare una superficie RGB: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] Uso conversione profondità/sp.colore, per questo sarà più lento (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] Formato immagine non supportato in draw_slice, contatta gli sviluppatori di MPlayer!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit fallito: %s.\n"
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] Inizializzazione SDL fallita: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] Uso il driver: %s.\n"

// libvo/vobsub_vidix.c
#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] Impossibile iniziare la riproduzione: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] Impossibile terminare la riproduzione: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] UV interlacciato per YUV410P non supportato.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] Chiamata alla funzione posticcia vidix_draw_slice().\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] Chiamata alla funzione posticcia vidix_draw_frame().\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] FourCC non supportato per questo driver VIDIX: %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] Il server grafico ha una risoluzione non supportata (%dx%d), supportate: %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] Il server grafico ha una profondità colore non supportata da vidix (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] Il driver VIDIX non può ingrandire l'immagine (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] Il driver VIDIX non può rimpicciolire l'immagine (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] Impossibile configurare la riproduzione: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] Hai la versione errata della libreria VIDIX.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] Impossibile trovare un driver VIDIX che possa funzionare.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] Impossibile ricavare caratteristiche: %s.\n"

// libvo/vo_svga.c
#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] Il vid_mode %d (%s) forzato non è disponibile.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] Il vid_mode %d (%s) è troppo piccolo.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Errore in vga_setmode(%d).\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] Video mode is linear and memcpy could be used for image transfer.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] Video mode has hardware acceleration and put_image could be used.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] If it works for you I would like to know.\n[VO_SVGA] (send log with `mplayer test.avi -v -v -v -v &> svga.log`). Thx!\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] La modalità video ha %d pagina/e.\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] Centro l'immagine. Partendo da (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] Uso VIDIX. l=%i a=%i  lm=%i am=%i\n"

// libvo/vo_tdfxfb.c
#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] Impossibile aprire %s: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] Problema con l'ioctl FBITGET_FSCREENINFO: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] Problema con l'ioctl FBITGET_VSCREENINFO: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] Questo driver supporta solo le 3Dfx Banshee, Voodoo3 e Voodoo 5.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] Uscita con %d bpp non supportata.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] Impossibile mappare le aree di memoria: %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] Uscita con %d bpp non supportata (CiÃÂ² non sarebbe dovuto accadere).\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Eik! Qualche problema in control().\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] Troppo poca memoria video per la riproduzione. Prova una risoluzione inferiore.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] screen è %dx%d a %d bpp, in è %dx%d a %d bpp, norm è %dx%d.\n"

// libvo/vo_tdfx_vid.c
#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Sposta %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] L'AGP move non ha potuto ripulire lo schermo.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit fallito.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] Al formato di overlay non nativo serve una conversione.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] Formato 0x%x in entrata non supportato.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Impostazione overlay non riuscita.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Attivazione overlay non riuscita.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Overlay pronto: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] Texture blit pronto: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Disattivazione overlay non riuscita\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] Impossibile aprire %s: %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] Impossibile ricavare la configurazione attuale: %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Errore in memmap !!!!!\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] Erroe in AGP move.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] Errore impostazione YUV.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP move non riuscita sul piano Y.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP move non riuscita sul piano U.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP move non riuscita sul piano V.\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] formato sconosciuto: 0x%x.\n"

// libvo/vo_tga.c
#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] Sottodispositivo sconosciuto: %s.\n"

// libvo/vo_vesa.c
#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] Errore fatale! Impossibile continuare.\n"
#define MSGTR_LIBVO_VESA_UnknownSubdevice "[VO_VESA] Sottodispositivo sconosciuto: '%s'.\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] Hai troppo poca memoria video per questa modalità:\n[VO_VESA] Richiesta: %08lX presente: %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] Devi specificare le caratteristiche del monitor. Non cambio frequenza di refresh.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] Modalità al di fuori dei limiti del monitor. Non cambio frequenza di refresh.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] Errore interno fatale rilevato: init eseguito prima di preinit.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] L'opzione -flip non è gestita.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] Ragione possibile: Nessun BIOS VBE2 trovato.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] Trovato BIOS VESA VBE Versione %x.%x Revisione: %x.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] Memoria video: %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] Caratteristiche VESA: %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! Informazioni OEM stampate qui sotto !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] Dovresti vedere qui sotto 5 righe relative all'OEM; Se no, hai una vm86 bacata.\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] Suggerimento: Per utilizzare l'uscita-TV devi avere il connettore TV attaccato\n"\
"[VO_VESA] prima dell'avvio visto che il BIOS VESA si inizializza solo durante il POST.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] Uso modalità VESA (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] Impossibile inizializzare il ridimensionatore sotfware.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] Impossibile usare DGA. Forzo la modalità 'bank switching mode'. :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] Uso DGA (risorse fisiche: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] Impossibile usare il double buffering: non abbastanza memoria video.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] Non riesco a trovare né DGA né un frame di finestra riposizionabile.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] Hai forzato DGA. Esco\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] Impossibile trovare un indirizzo finestra valido.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] Uso la modalità 'bank switching mode' (risorse fisiche: %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] Imposibile allocare un buffer temporaneo.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] Spiacente, modalità non supportata -- prova -x 640 -zoom.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] Oh, hai davvero un'immagine sulla Tivu!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] Impossibile inizializzare l'Overlay Video Linux.\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] Uso l'overlay video: %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] Impossibile inizializzare il driver VIDIX.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] Uso VIDIX.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] Impossibile trovare una modalità per: %ux%u@%u.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] Inizializzazione VESA completa.\n"

// libvo/vo_x11.c
#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] chiamata a draw_frame()!!!!!!\n"

// libvo/vo_xv.c
#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] chiamata a draw_frame()!!!!!!\n"
#define MSGTR_LIBVO_XV_SharedMemoryNotSupported "[VO_XV] Memoria condivisa non supportata\nRitorno a Xv normale.\n"
#define MSGTR_LIBVO_XV_XvNotSupportedByX11 "[VO_XV] Scusa, Xv non è gestito dalla versione/driver di X11 corrente\n[VO_XV] ******** Prova con  -vo x11  o  -vo sdl  *********\n"
#define MSGTR_LIBVO_XV_XvQueryAdaptorsFailed  "[VO_XV] Funzione XvQueryAdaptors fallita.\n"
#define MSGTR_LIBVO_XV_InvalidPortParameter "[VO_XV] Parametro porta non valido, lo sovrascrivo con la porta 0.\n"
#define MSGTR_LIBVO_XV_CouldNotGrabPort "[VO_XV] Impossibile aprire la porta %i.\n"
#define MSGTR_LIBVO_XV_CouldNotFindFreePort "[VO_XV] Impossibile trovare una porta Xvideo libera - forse un altro processo\n"\
"[VO_XV] la sta già usando. Chiudi tutte le applicazioni video, e riprova.\n"\
"[VO_XV] Se ciò non ti aiuta, vedi 'mplayer -vo help' per altri (non-xv)\n"\
"[VO_XV] driver di uscita video.\n"
#define MSGTR_LIBVO_XV_NoXvideoSupport "[VO_XV] Sembra non sia disponibile il supporto Xvideo per la tua scheda video.\n"\
"[VO_XV] Esegui 'xvinfo' per verificare il supporto Xv e leggi\n"\
"[VO_XV] DOCS/HTML/it/video.html#xv!\n"\
"[VO_XV] Vedi 'mplayer -vo help' per altri (non-xv) driver di uscita video.\n"\
"[VO_XV] Prova con -vo x11.\n"
 

// loader/ldt_keeper.c
#define MSGTR_LOADER_DYLD_Warning "ATTENZIONE: Tentativo di utilizzare codec DLL, senza la variabile d'ambiente\n           DYLD_BIND_AT_LAUNCH impostata. Ciò porterà probabilmente a un crash.\n"


// stream/stream_radio.c
#define MSGTR_RADIO_ChannelNamesDetected "[radio] Rilevati i nomi dei canali radio.\n"
#define MSGTR_RADIO_FreqRange "[radio] La gamma delle frequenze permesse è %.2f-%.2f MHz.\n"
#define MSGTR_RADIO_WrongFreqForChannel "[radio] Frequenza errata per il canale %s\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[radio] Numero canale errato: %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[radio] Numero canale errato: %d\n"
#define MSGTR_RADIO_WrongChannelName "[radio] Nome canale errato: %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[radio] Rilevato parametro frequenza radio.\n"
#define MSGTR_RADIO_DoneParsingChannels "[radio] Lettura canali terminata.\n"
#define MSGTR_RADIO_GetTunerFailed "[radio] Attenzione: ioctl tuner fallito: %s. Imposto frac a %d.\n"
#define MSGTR_RADIO_NotRadioDevice "[radio] %s non è un dispositivo radio!\n"
#define MSGTR_RADIO_TunerCapLowYes "[radio] il tuner è low:yes frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[radio] il tuner è low:no frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[radio] ioctl impostazione frequenza 0x%x (%.2f) fallito: %s\n"
#define MSGTR_RADIO_GetFreqFailed "[radio] ioctl rilevazione frequenza fallito: %s\n"
#define MSGTR_RADIO_SetMuteFailed "[radio] ioctl impostazione muto fallito: %s\n"
#define MSGTR_RADIO_QueryControlFailed "[radio] ioctl query control fallito: %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[radio] ioctl irilevazione volume fallito: %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[radio] ioctl impostazione volume fallito: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[radio] too bad - dropping audio frame (%d byte)!\n"
#define MSGTR_RADIO_BufferEmpty "[radio] grab_audio_frame: buffer vuoto, aspetto %d byte di dati.\n"
#define MSGTR_RADIO_AudioInitFailed "[radio] audio_in_init fallito: %s\n"
#define MSGTR_RADIO_AudioBuffer "[radio] Cattura audio - buffer=%d byte (blocco=%d byte).\n"
#define MSGTR_RADIO_AllocateBufferFailed "[radio] impossibile allocare il buffer audio (blocco=%d,buf=%d): %s\n"
#define MSGTR_RADIO_CurrentFreq "[radio] Frequenza attuale: %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[radio] Canale selezionato: %d - %s (freq: %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[radio] Impossibile cambiare canale: nessuna lista canali fornita.\n"
#define MSGTR_RADIO_UnableOpenDevice "[radio] Impossibile aprire '%s': %s\n"
#define MSGTR_RADIO_InitFracFailed "[radio] init_frac fallito.\n"
#define MSGTR_RADIO_WrongFreq "[radio] Frequenza errata: %.2f\n"
#define MSGTR_RADIO_UsingFreq "[radio] Uso la frequenza: %.2f.\n"
#define MSGTR_RADIO_AudioInInitFailed "[radio] audio_in_init fallito.\n"
#define MSGTR_RADIO_BufferString "[radio] %s: in buffer=%d scartati=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[radio] Chiamata a audio_in_setup fallita: %s\n"
#define MSGTR_RADIO_CaptureStarting "[radio] Inizio la parte di cattura.\n"
#define MSGTR_RADIO_ClearBufferFailed "[radio] Pulizia buffer fallita: %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[radio] Chiamata a stream_enable_cache fallita: %s\n"
#define MSGTR_RADIO_DriverUnknownStr "[radio] Nome driver sconosciuto: %s\n"
#define MSGTR_RADIO_DriverV4L2 "[radio] Uso l'interfaccia radio V4Lv2.\n"
#define MSGTR_RADIO_DriverV4L "[radio] Uso l'interfaccia radio V4Lv1.\n"
#define MSGTR_RADIO_DriverBSDBT848 "[radio] Utilizzo l'interfaccia radio *BSD BT848.\n"
#define MSGTR_RADIO_AvailableDrivers "[radio] Driver disponibili: "


// ================================== LIBASS ====================================

// ass_bitmap.c
#define MSGTR_LIBASS_FT_Glyph_To_BitmapError "[ass] Errore FT_Glyph_To_Bitmap %d \n"
#define MSGTR_LIBASS_UnsupportedPixelMode "[ass] Modalità pixel non supportata: %d\n"
#define MSGTR_LIBASS_GlyphBBoxTooLarge "[ass] Glyph Bounding Box troppo ampio: %dx%dpx\n"

// ass.c
#define MSGTR_LIBASS_NoStyleNamedXFoundUsingY "[ass] [%p] Attenzione: non trovato alcuno stile chiamato '%s', uso '%s'\n"
#define MSGTR_LIBASS_BadTimestamp "[ass] timestamp errato\n"
#define MSGTR_LIBASS_BadEncodedDataSize "[ass] dimensione dati codificati errata\n"
#define MSGTR_LIBASS_FontLineTooLong "[ass] Linea font troppo lunga: %d, %s\n"
#define MSGTR_LIBASS_EventFormatHeaderMissing "[ass] Testata formato eventi mancante\n"
#define MSGTR_LIBASS_ErrorOpeningIconvDescriptor "[ass] errore nell'apertura del descrittore iconv.\n"
#define MSGTR_LIBASS_ErrorRecodingFile "[ass] errore ricodificando il file.\n"
#define MSGTR_LIBASS_FopenFailed "[ass] ass_read_file(%s): fopen fallita\n"
#define MSGTR_LIBASS_FseekFailed "[ass] ass_read_file(%s): fseek fallita\n"
#define MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan10M "[ass] ass_read_file(%s): Mi rifiuto di caricare sottotitoli più grandi di 10M\n"
#define MSGTR_LIBASS_ReadFailed "Lettura fallita, %d: %s\n"
#define MSGTR_LIBASS_AddedSubtitleFileMemory "[ass] Aggiunto file sottotitoli: <memoria> (%d stili, %d eventi)\n"
#define MSGTR_LIBASS_AddedSubtitleFileFname "[ass] Added subtitle file: %s (%d stili, %d eventi)\n"
#define MSGTR_LIBASS_FailedToCreateDirectory "[ass] Impossibile creare la directory %s\n"
#define MSGTR_LIBASS_NotADirectory "[ass] Non è una directory: %s\n"

// ass_cache.c
#define MSGTR_LIBASS_TooManyFonts "[ass] Troppi font\n"
#define MSGTR_LIBASS_ErrorOpeningFont "[ass] Errore aprendo il font: %s, %d\n"

// ass_fontconfig.c
#define MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne "[ass] fontconfig: Il font selezionato non è quello richiesto: '%s' != '%s'\n"
#define MSGTR_LIBASS_UsingDefaultFontFamily "[ass] fontconfig_select: Uso la famiglia di default dei font: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingDefaultFont "[ass] fontconfig_select: Uso il font di default: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingArialFontFamily "[ass] fontconfig_select: Uso la famiglia dei font 'Arial': (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed "[ass] FcInitLoadConfigAndFonts fallita.\n"
#define MSGTR_LIBASS_UpdatingFontCache "[ass] Aggiornamento cache dei font.\n"
#define MSGTR_LIBASS_BetaVersionsOfFontconfigAreNotSupported "[ass] Le versioni beta di fontconfig non sono supportate.\n[ass] Esegui un aggiornamento prima di riportare dei bug.\n"
#define MSGTR_LIBASS_FcStrSetAddFailed "[ass] FcStrSetAdd fallita.\n"
#define MSGTR_LIBASS_FcDirScanFailed "[ass] FcDirScan fallita.\n"
#define MSGTR_LIBASS_FcDirSave "[ass] FcDirSave fallita.\n"
#define MSGTR_LIBASS_FcConfigAppFontAddDirFailed "[ass] FcConfigAppFontAddDir fallita\n"
#define MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed "[ass] Fontconfig disabilitato, sarà usato solo il font di default.\n"
#define MSGTR_LIBASS_FunctionCallFailed "[ass] %s fallita\n"

// ass_render.c
#define MSGTR_LIBASS_NeitherPlayResXNorPlayResYDefined "[ass] Non son definite PlayResX né PlayResY. Assumo 384x288.\n"
#define MSGTR_LIBASS_PlayResYUndefinedSettingY "[ass] PlayResY non definita, la imposto a %d.\n"
#define MSGTR_LIBASS_PlayResXUndefinedSettingX "[ass] PlayResX non definita, la imposto a %d.\n"
#define MSGTR_LIBASS_FT_Init_FreeTypeFailed "[ass] FT_Init_FreeType fallita.\n"
#define MSGTR_LIBASS_Init "[ass] Init\n"
#define MSGTR_LIBASS_InitFailed "[ass] Init fallito.\n"
#define MSGTR_LIBASS_BadCommand "[ass] Comando errato: %c%c\n"
#define MSGTR_LIBASS_ErrorLoadingGlyph  "[ass] Errore nel carcamento glifo.\n"
#define MSGTR_LIBASS_FT_Glyph_Stroke_Error "[ass] errore %d in FT_Glyph_Stroke\n"
#define MSGTR_LIBASS_UnknownEffectType_InternalError "[ass] Tipo di effetto sconosciuto (errore interno)\n"
#define MSGTR_LIBASS_NoStyleFound "[ass] Nessuno stile trovato!\n"
#define MSGTR_LIBASS_EmptyEvent "[ass] Evento vuoto!\n"
#define MSGTR_LIBASS_MAX_GLYPHS_Reached "[ass] Raggiunto MAX_GLYPHS: evento %d, inizio = %llu, durata = %llu\n Testo = %s\n"
#define MSGTR_LIBASS_EventHeightHasChanged "[ass] Attenzione! L'evento altezza è cambiato!  \n"

// ass_font.c
#define MSGTR_LIBASS_GlyphNotFoundReselectingFont "[ass] Glifo 0x%X non trovato, seleziono un altro font per (%s, %d, %d)\n"
#define MSGTR_LIBASS_GlyphNotFound "[ass] Glifo 0x%X non trovato nel font per (%s, %d, %d)\n"
#define MSGTR_LIBASS_ErrorOpeningMemoryFont "[ass] Errore aprendo il font in memoria: %s\n"
#define MSGTR_LIBASS_NoCharmaps "[ass] font face senza alcuna mappa caratteri\n"
#define MSGTR_LIBASS_NoCharmapAutodetected "[ass] nessuna mappa caratteri rilevata automaticamente, provo la prima\n"

//tv.c
#define MSGTR_TV_BogusNormParameter "tv.c: norm_from_string(%s): Parametro norm invalido, lo imposto a %s.\n"
#define MSGTR_TV_NoVideoInputPresent "Errore: Nessuna entrata video presente!\n"
#define MSGTR_TV_UnknownImageFormat ""\
"========================================================================\n"\
" ATTENZIONE: FORMATO IMMAGINE DI USCITA NON TESTATO O SCONOSCIUTO (0x%x)\n"\
" CiÃÂ² potrebbe causare una riproduzione errata o bloccare il programma!\n"\
" I bug report saranno ignorati! Dovresti riprovare con YV12 (che è lo\n"\
" spazio colore di default) e leggere la documentazione!\n"\
"========================================================================\n"
#define MSGTR_TV_SelectedNormId "Selezionato id norma: %d\n"
#define MSGTR_TV_SelectedNorm "Selezionata norma : %s\n"
#define MSGTR_TV_CannotSetNorm "Errore: Impossibile impostare la norma!\n"
#define MSGTR_TV_MJP_WidthHeight "  MJP: larghezza (width) %d altezza (height) %d\n"
#define MSGTR_TV_UnableToSetWidth "Non riesco ad impostare la larghezza richiesta: %d\n"
#define MSGTR_TV_UnableToSetHeight "Non riesco ad impostare l'altezza richiesta: %d\n"
#define MSGTR_TV_NoTuner "L'input scelto non ha un sintonizzatore/tuner!\n"
#define MSGTR_TV_UnableFindChanlist "Impossibile trovare la lista di canali indicata! (%s)\n"
#define MSGTR_TV_SelectedChanlist "Selezionata lista canali: %s (contenente %d canali)\n"
#define MSGTR_TV_ChannelFreqParamConflict "Non puoi impostare frequenza e canale contemporaneamente!\n"
#define MSGTR_TV_ChannelNamesDetected "Rilevati nomi dei canali TV.\n"
#define MSGTR_TV_NoFreqForChannel "Non riesco a trovare la frequenza per il canale %s (%s)\n"
#define MSGTR_TV_SelectedChannel3 "Scelto canale: %s - %s (freq: %.3f)\n"
#define MSGTR_TV_SelectedChannel2 "Scelto canale: %s (freq: %.3f)\n"
#define MSGTR_TV_SelectedFrequency "Scelta frequenza: %lu (%.3f)\n"
#define MSGTR_TV_RequestedChannel "Canale richiesto: %s\n"
#define MSGTR_TV_UnsupportedAudioType "Tipo audio '%s (%x)' non supportato!\n"
#define MSGTR_TV_AudioFormat "  TV audio: %d canali, %d bit, %d Hz\n"
#define MSGTR_TV_AvailableDrivers "Driver disponibili:\n"
#define MSGTR_TV_DriverInfo "Scelto driver: %s\n nome: %s\n autore: %s\n commento: %s\n"
#define MSGTR_TV_NoSuchDriver "Driver non trovato: %s\n"
#define MSGTR_TV_DriverAutoDetectionFailed "Rilevazione automatica del driver TV fallita.\n"
#define MSGTR_TV_UnknownColorOption "Specificata un'opzione colore (%d) sonosciuta!\n"
#define MSGTR_TV_CurrentFrequency "Frequenza attuale: %lu (%.3f)\n"
#define MSGTR_TV_NoTeletext "Segnale televideo assente"
#define MSGTR_TV_Bt848IoctlFailed "tvi_bsdbt848: Chiamata a ioctl %s fallita. Errore: %s\n"
#define MSGTR_TV_Bt848InvalidAudioRate "tvi_bsdbt848: Frequenza audio non valida. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorOpeningBktrDev "tvi_bsdbt848: Impossibile aprire il dispositivo bktr. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorOpeningTunerDev "tvi_bsdbt848: Impossibile aprire il dispositivo del tuner. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorOpeningDspDev "tvi_bsdbt848: Impossibile aprire il dispositivo dsp. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorConfiguringDsp "tvi_bsdbt848: Configurazione di dsp fallita. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorReadingAudio "tvi_bsdbt848: Errore leggendo i dati audio. Errore: %s\n"
#define MSGTR_TV_Bt848MmapFailed "tvi_bsdbt848: mmap fallita. Errore: %s\n"
#define MSGTR_TV_Bt848FrameBufAllocFailed "tvi_bsdbt848: Allocazione frame buffer non riuscita. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorSettingWidth "tvi_bsdbt848: Errore impostando la larghezza immagine. Errore: %s\n"
#define MSGTR_TV_Bt848ErrorSettingHeight "tvi_bsdbt848: Errore impostando l'altezza immagine. Errore: %s\n"
#define MSGTR_TV_Bt848UnableToStopCapture "tvi_bsdbt848: Impossibile interrompere l'aquisizione. Errore: %s\n"
#define MSGTR_TV_TTSupportedLanguages "Lingue del televideo utilizzabili:\n"
#define MSGTR_TV_TTSelectedLanguage "Selezionata la lingua di default del televideo: %s\n"
#define MSGTR_TV_ScannerNotAvailableWithoutTuner "La scansione automatica canali non è disponibile senza un sintonizzatore\n"

//tvi_dshow.c
#define MSGTR_TVI_DS_UnableConnectInputVideoDecoder  "Impossibile collegare al decodificatore video l'ingresso indicato. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableConnectInputAudioDecoder  "Impossibile collegare al decodificatore audio l'ingresso indicato. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableSelectVideoFormat "tvi_dshow: Impossibile selezionare il formato video. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableSelectAudioFormat "tvi_dshow: Impossibile selezionare il formato audio. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableGetMediaControlInterface "tvi_dshow: Impossibile ricavare l'interfaccia IMediaControl. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableStartGraph "tvi_dshow: Impossibile avviare graph! Errore:0x%x\n"
#define MSGTR_TVI_DS_DeviceNotFound "tvi_dshow: Dispositivo #%d non trovato\n"
#define MSGTR_TVI_DS_UnableGetDeviceName "tvi_dshow: Impossibile ricavare il nome per il dispositivo #%d\n"
#define MSGTR_TVI_DS_UsingDevice "tvi_dshow: Uso il dispositivo #%d: %s\n"
#define MSGTR_TVI_DS_DeviceName  "tvi_dshow: Dispositivo #%d: %s\n"
#define MSGTR_TVI_DS_DirectGetFreqFailed "tvi_dshow: Impossibile ricavare direttamente la frequenza. Sarà usata la la tabella di sistema dei canali.\n"
#define MSGTR_TVI_DS_DirectSetFreqFailed "tvi_dshow: Impossibile impostare direttamente la frequenza. Sarà usata la la tabella di sistema dei canali.\n"
#define MSGTR_TVI_DS_SupportedNorms "tvi_dshow: norme disponibili:"
#define MSGTR_TVI_DS_AvailableVideoInputs "tvi_dshow: ingressi video disponibili:"
#define MSGTR_TVI_DS_AvailableAudioInputs "tvi_dshow: ingressi audio disponibili:"
//following phrase will be printed near the selected audio/video input
#define MSGTR_TVI_DS_InputSelected "(selezionato)"
#define MSGTR_TVI_DS_UnableExtractFreqTable "tvi_dshow: Impossibile leggere la tabella delle frequenze da kstvtune.ax\n"
#define MSGTR_TVI_DS_WrongDeviceParam "tvi_dshow: Parametro device errato: %s\n"
#define MSGTR_TVI_DS_WrongDeviceIndex "tvi_dshow: Indice di device errato: %d\n"
#define MSGTR_TVI_DS_WrongADeviceParam "tvi_dshow: Parametro adevice errato: %s\n"
#define MSGTR_TVI_DS_WrongADeviceIndex "tvi_dshow: Indice di adevice errato: %d\n"

#define MSGTR_TVI_DS_SamplerateNotsupported "tvi_dshow: Campionamento %d non supportato dal dispositivo. Provo con il primo disponibile.\n"
#define MSGTR_TVI_DS_VideoAdjustigNotSupported "tvi_dshow: Impostazione di luminosità/tonalità/saturazione/contrasto non disponibile per il dispositivo\n"

#define MSGTR_TVI_DS_ChangingWidthHeightNotSupported "tvi_dshow: Modifica di larghezza/altezza video non disponibile per il dispositivo.\n"
#define MSGTR_TVI_DS_SelectingInputNotSupported  "tvi_dshow: Selezione della sorgente di acquisizione non disponibile per il dispositivo.\n"
#define MSGTR_TVI_DS_FreqTableLoaded "tvi_dshow: caricata la tabella delle frequenze di sistema (%s) per la zona id=%d (canali:%d).\n"
#define MSGTR_TVI_DS_ErrorParsingAudioFormatStruct "tvi_dshow: Impossibile interpretare la struttura del formato audio.\n"
#define MSGTR_TVI_DS_ErrorParsingVideoFormatStruct "tvi_dshow: Impossibile interpretare la struttura del formato video.\n"
#define MSGTR_TVI_DS_UnableSetAudioMode "tvi_dshow: Impossibile impostare la modalità audio %d. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnsupportedMediaType "tvi_dshow: Tipo di media non gestibile passato a %s\n"
#define MSGTR_TVI_DS_UnableGetsupportedVideoFormats "tvi_dshow: Impossibile ricavare dal pin video i formati media supportati. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableGetsupportedAudioFormats "tvi_dshow: Impossibile ricavare dal pin audio i formati media supportati. Errore:0x%x Disabilito l'audio.\n"
#define MSGTR_TVI_DS_UnableFindNearestChannel "tvi_dshow: Impossibile trovare il prossimo canale nella tabella di frequenze di sistema.\n"
#define MSGTR_TVI_DS_UnableToSetChannel "tvi_dshow: Impossibile passare al prossimo canale nella tabella di frequenze di sistema. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableTerminateVPPin "tvi_dshow: Impossibile chiudere il pin VideoPort pin con alcun filtro in graph. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableBuildVideoSubGraph "tvi_dshow: Impossibile costruire la catena video di acquisizione graph. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableBuildAudioSubGraph "tvi_dshow: Impossibile costruire la catena audio di acquisizione graph. Errore:0x%x\n"
#define MSGTR_TVI_DS_UnableBuildVBISubGraph "tvi_dshow: Impossibile costruire la catena VBI di acquisizione graph. Errore:0x%x\n"
#define MSGTR_TVI_DS_GraphInitFailure "tvi_dshow: Inizializzazione graph di Directshow fallita.\n"
#define MSGTR_TVI_DS_NoVideoCaptureDevice "tvi_dshow: Impossibile trovare il dispositivo di acquisizione video.\n"
#define MSGTR_TVI_DS_NoAudioCaptureDevice "tvi_dshow: Impossibile trovare il dispositivo di acquisizione audio.\n"
#define MSGTR_TVI_DS_GetActualMediatypeFailed "tvi_dshow: Impossibile ricavare il mediatype attuale (Errore:0x%x). Assumo sia uguale a quello richiesto.\n"
