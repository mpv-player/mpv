// Translated by: Fabio Olimpieri <fabio.olimpieri@tin.it>
// Updated by: Roberto Togni <see AUTHORS for email address>
// Updated by: PaulTT <paultt@hackerjournal.it>

// Updated to help_mp-en.h v1.173 (still missing some messages)

// TODO: change references to DOCS/HTML/en/... to DOCS/HTML/it/... when they will be updated
//
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Uso:   mplayer [opzioni] [url|percorso/]nome_file\n"
"\n"
"Opzioni di base: (vedi la pagina man per la lista completa)\n"
" -vo <drv[:dev]>  sceglie driver e dispositivo uscita video ('-vo help' lista)\n"
" -ao <drv[:dev]>  sceglie driver e dispositivo uscita audio ('-ao help' lista)\n"
#ifdef HAVE_VCD
" vcd://<trackno>  legge (S)VCD (Super Video CD) (dispositivo raw, non montato)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  legge titolo/traccia DVD dal dispositivo anziché da file\n"
" -alang/-slang    sceglie lingua audio/sottotitoli DVD (cod naz. 2 caratteri)\n"
#endif
" -ss <timepos>    cerca una determinata posizione (in secondi o in hh:mm:ss) \n"
" -nosound         non riproduce l\'audio\n"
" -fs              opzioni schermo intero (o -vm, -zoom, vedi pagina man)\n"
" -x <x> -y <y>    imposta la risoluzione dello schermo (usare con -vm o -zoom)\n"
" -sub <file>      file sottotitoli da usare (vedi anche -subfps, -subdelay)\n"
" -playlist <file> specifica il file della playlist\n"
" -vid x -aid y    seleziona il flusso video (x) ed audio (y) da riprodurre\n"
" -fps x -srate y  cambia il rate del video (x fps) e dell\'audio (y Hz)\n"
" -pp <quality>    abilita filtro postelaborazione (vedi pagina man x dettagli)\n"
" -framedrop       abilita lo scarto dei fotogrammi (per macchine lente)\n"
"\n"
"Tasti principali: (vedi pagina man per lista, controlla anche input.conf)\n"
" <-  o  ->        va indietro/avanti di 10 secondi\n"
" su o giù         va indietro/avanti di  1 minuto\n"
" pagsu o paggiù   va indietro/avanti di 10 minuti\n"
" < o >            va indietro/avanti nella playlist\n"
" p o SPAZIO       pausa (premere un qualunque tasto per continuare)\n"
" q o ESC          ferma la riproduzione ed esce dal programma\n"
" + o -            regola il ritardo audio di +/- 0.1 secondi\n"
" o                modalità OSD: niente / barra ricerca / barra ricerca + tempo\n"
" * o /            incrementa o decrementa il volume PCM\n"
" z o x            regola il ritardo dei sottotitoli di +/- 0.1 secondi\n"
" r o t            posizione alto/basso dei sottotitoli, vedi anche -vf expand\n"
"\n"
" * * * VEDI PAGINA MAN PER DETTAGLI, ULTERIORI OPZIONI AVANZATE E TASTI! * * *\n"
"\n";
#endif

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
#define MSGTR_InvalidVOdriver "Nome del diver video di output non valido: %s\nUsa '-vo help' per avere una lista dei driver audio disponibili.\n"
#define MSGTR_InvalidAOdriver "Nome del diver audio di output non valido: %s\nUsa '-ao help' per avere una lista dei driver audio disponibili.\n"
#define MSGTR_CopyCodecsConf "(copia/linka etc/codecs.conf dai sorgenti di MPlayer a ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Utilizzo la versione interna predefinita di codecs.conf\n"
#define MSGTR_CantLoadFont "Impossibile caricare i font: %s\n"
#define MSGTR_CantLoadSub "Impossibile caricare i sottotitoli: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: manca il flusso selezionato!\n"
#define MSGTR_CantOpenDumpfile "Impossibile aprire il file di dump!!!\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS non specificato (o non valido) nell\'intestazione! Usa l\'opzione -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Cerco di forzare l\'uso della famiglia dei driver dei codec audio %d...\n"
#define MSGTR_CantFindAudioCodec "Impossibile trovare il codec per il formato audio 0x%X!\n"
#define MSGTR_RTFMCodecs "Leggi DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Cerco di forzare l\'uso della famiglia dei driver dei codec video %d...\n"
#define MSGTR_CantFindVideoCodec "Impossibile trovare il codec per il formato video 0x%X!\n"
#define MSGTR_CannotInitVO "FATALE: Impossibile inizializzare il driver video!\n"
#define MSGTR_CannotInitAO "Impossibile aprire/inizializzare il dispositivo audio -> NESSUN SUONO\n"
#define MSGTR_StartPlaying "Inizio la riproduzione...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"       ****************************************************************\n"\
"       **** Il tuo sistema è troppo lento per questa riproduzione! ****\n"\
"       ****************************************************************\n"\
"Possibili cause, problemi, soluzioni:\n"\
"- Nella maggior parte dei casi: driver _audio_ danneggiato/bacato\n"\
"  - Prova -ao sdl o usa l\'emulazione OSS di ALSA.\n"\
"  - Puoi anche provare con diversi valori di -autosync, 30 e' un buon inizio.\n"\
"- Output video lento\n"\
"  - Prova un altro -vo driver (-vo help per la lista) o prova con -framedrop!\n"\
"- Cpu lenta\n"\
"  - Non provare a guardare grossi DVD/DivX su cpu lente! Prova -hardframedrop.\n"\
"- File rovinato\n"\
"  - Prova varie combinazioni di -nobps -ni -forceidx -mc 0.\n"\
"- Dispositivo lento (punti di mount NFS/SMB, DVD, VCD etc)\n"\
"  - Prova -cache 8192.\n"\
"- Stai usando -cache per riprodurre un file AVI senza interleave?\n"\
"  - Prova con -nocache.\n"\
"Leggi DOCS/HTML/en/video.html per suggerimenti su regolazione/accelerazione.\n"\
"Se nulla di ciò ti aiuta, allora leggi DOCS/HTML/en/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer è stato compilato senza il supporto della GUI!\n"
#define MSGTR_GuiNeedsX "LA GUI di MPlayer richiede X11!\n"
#define MSGTR_Playing "Riproduco %s\n"
#define MSGTR_NoSound "Audio: nessun suono!!!\n"
#define MSGTR_FPSforced "FPS forzato a %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection ""\
"Compilato con riconoscimento CPU in esecuzione - ATTENZIONE - non è l'optimus!\n"\
"Per avere le migliori prestazioni, ricompila MPlayer con\n"\
" --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Compilato per CPU x86 con estensioni:"
#define MSGTR_AvailableVideoOutputDrivers "Driver di output video disponibili:\n"
#define MSGTR_AvailableAudioOutputDrivers "Driver di output audio disponibili:\n"
#define MSGTR_AvailableAudioCodecs "Codec audio disponibili:\n"
#define MSGTR_AvailableVideoCodecs "Codec video disponibili:\n"
#define MSGTR_AvailableAudioFm "\nFamiglie/driver di codec audio disponibili (compilati):\n"
#define MSGTR_AvailableVideoFm "\nFamiglie/driver di codec video disponibili (compilati):\n"
#define MSGTR_AvailableFsType "Modi disponibili a schermo pieno:\n"
#define MSGTR_UsingRTCTiming "Sto utilizzando la temporizzazione hardware RTC di Linux (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: impossibile leggere le proprietà\n"
#define MSGTR_NoStreamFound "Nessun flusso trovato\n"
#define MSGTR_ErrorInitializingVODevice "Errore aprendo/inizializzando il dispositivo uscita video (-vo) selezionato!\n"
#define MSGTR_ForcedVideoCodec "Codec video forzato: %s\n"
#define MSGTR_ForcedAudioCodec "Codec audio forzato: %s\n"
#define MSGTR_Video_NoVideo "Video: no video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATALE: Impossibile inizializzare i filtri video (-vf) o l'output video (-vo)!\n"
#define MSGTR_Paused "\n  =====  PAUSA  =====\r"
#define MSGTR_PlaylistLoadUnable "\nImpossibile caricare la playlist %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer è stato interrotto dal segnale 'Istruzione illegale'.\n"\
"  Potrebbe essere un errore nel codice di rilevamento tipo di processore...\n"\
"  leggi DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer è stato interrotto dal segnale 'Istruzione illegale'.\n"\
"  Solitamente questo avviene quando si esegue il programma su un processore\n"\
"  diverso da quello per cui è stato compilato/ottimizzato.\n"\
"  Verificalo!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer è stato interrotto per un errore nell'uso della CPU/FPU/RAM.\n"\
"  Ricompila MPlayer con --enable-debug e crea un backtrace ed un disassemblato\n"\
"  con 'gdb'. Per dettagli DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer è andato in crash. Questo non dovrebbe accadere.\n"\
"  Può essere un errore nel codice di MPlayer _o_ nei tuoi driver _o_ nella tua\n"\
"  versione di gcc. Se ritieni sia colpa di MPlayer, perfavore leggi\n"\
"  DOCS/HTML/en/bugreports.html e segui quelle istruzioni. Non possiamo\n"\
"  aiutarti, e non lo faremo, se non ci dai queste informazioni quando segnali\n"\
"  un possibile problema.\n"
#define MSGTR_LoadingConfig "Carico configurazione '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: aggiunto file sottotitoli (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: rimosso file sottotitoli (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Errore durante l'apertura del file [%s] per la scrittura!\n"
#define MSGTR_CommandLine "CommandLine:"
#define MSGTR_RTCDeviceNotOpenable "Fallimento nell'aprire %s: %s (dovrebbe esser leggibile dall'utente.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC: errore di init in ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Prova aggiungendo \"echo %lu > /proc/sys/dev/rtc/max-user-freq\"\n"\
"agli script di avvio del sistema.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC: errore di init in ioctl (rtc_pie_on): %s\n"
#define MSGTR_MenuInitialized "Menu inizializzato: %s\n"
#define MSGTR_MenuInitFailed "Inizializzazione Menu fallita.\n"
#define MSGTR_Getch2InitializedTwice "WARNING: getch2_init chiamata 2 volte!\n"
#define MSGTR_DumpstreamFdUnavailable "Non posso fare il dump di questo stream - nessun 'fd' disponibile.\n"
#define MSGTR_FallingBackOnPlaylist "Provo infine a interpretare playlist %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Non riesco ad aprire il filtro video libmenu col menu base %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Errore nel pre-init della sequenza di filtri audio!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC: errore di lettura: %s\n"

#define MSGTR_EdlCantUseBothModes "Non puoi usare -edl e -edlout contemporanemente.\n"
#define MSGTR_EdlOutOfMem "Non posso allocare abbastanza memoria per i dati EDL.\n"
#define MSGTR_EdlRecordsNo "Lette azioni EDL %d.\n"
#define MSGTR_EdlQueueEmpty "Non ci sono azioni EDL di cui curarsi.\n"
#define MSGTR_EdlCantOpenForWrite "Non posso aprire il file EDL [%s] per la scrittura.\n"
#define MSGTR_EdlCantOpenForRead "Non posso aprire il file EDL [%s] per la lettura.\n"
#define MSGTR_EdlNOsh_video "Non posso usare EDL senza video, disabilitate.\n"
#define MSGTR_EdlNOValidLine "Linea EDL invalida: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Linea EDL scritta male [%d]: la ignoro.\n"
// TODO: overlap = ?
#define MSGTR_EdlBadLineOverlap "L'ultimo stop era a [%f]; lo start successivo a [%f].\n"\
"Le indicazioni devono essere cronologiche, non posso overlap. Ignoro.\n"
#define MSGTR_EdlBadLineBadStop "Lo stop deve essere dopo il tempo di start.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Sto usando il file di controllo passo3: %s\n"
#define MSGTR_MissingFilename "\nNome file mancante.\n\n"
#define MSGTR_CannotOpenFile_Device "Impossibile aprire il file/dispositivo.\n"
#define MSGTR_CannotOpenDemuxer "Impossibile aprire il demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nNessun encoder audio (-oac) scelto! Scegline uno (vedi -oac help) o -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nNessun encoder video (-ovc) scelto! Selezionane uno (vedi -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Impossibile aprire il file di output '%s'.\n"
#define MSGTR_EncoderOpenFailed "Errore nell'apertura dell'encoder.\n"
#define MSGTR_ForcingOutputFourcc "Forzo il fourcc di output a %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "Forzo la tag del formato audio a 0x%x\n"
#define MSGTR_WritingAVIHeader "Scrittura intestazione AVI...\n"
#define MSGTR_DuplicateFrames "\n%d frame(s) duplicato/i!!!    \n"
#define MSGTR_SkipFrame "\nScarto fotogramma!\n"
#define MSGTR_ResolutionDoesntMatch "\nIl nuovo file video ha diversa risoluzione o spazio colori dal precedente.\n"
#define MSGTR_FrameCopyFileMismatch "\nTutti i file video devono avere stessi fps, risoluz., e codec per -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nTutti i file devono avere lo stesso codec audio e formato per -oac copy.\n"
#define MSGTR_NoSpeedWithFrameCopy "WARNING: -speed non è detto che funzioni correttamente con -oac copy!\n"\
"L'encoding potrebbe essere danneggiato!\n"
#define MSGTR_ErrorWritingFile "%s: errore nella scrittura del file.\n"
#define MSGTR_WritingAVIIndex "\nScrittura indice AVI...\n"
#define MSGTR_FixupAVIHeader "Completamento intestazione AVI...\n"
#define MSGTR_RecommendedVideoBitrate "Il bitrate video consigliato per %s CD è: %d\n"
#define MSGTR_VideoStreamResult "\nFlusso video: %8.3f kbit/s  (%d bps)  dim.: %d bytes  %5.3f sec   %d frames\n"
#define MSGTR_AudioStreamResult "\nFlusso audio: %8.3f kbit/s  (%d bps)  dim.: %d bytes  %5.3f secondi\n"
#define MSGTR_OpenedStream "successo: formato: %d  dati: 0x%X - 0x%x\n"
#define MSGTR_CBRPCMAudioSelected "CBR PCM audio selezionato\n"
#define MSGTR_MP3AudioSelected "MP3 audio selezionato\n"
#define MSGTR_CannotAllocateBytes "Non posso allocare %d bytes\n"
#define MSGTR_SettingAudioDelay "Setto l'AUDIO DELAY a %5.3f\n"
#define MSGTR_SettingAudioInputGain "Setto l'audio input gain a %f\n"
#define MSGTR_LimitingAudioPreload "Limito il preload audio a 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Aumento la densità audio a 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Forzo il preload audio a 0, max pts correction a 0\n"
#define MSGTR_InvalidBitrateForLamePreset ""\
"Errore: il bitrate specificato è fuori gamma per questo Preset\n"\
"\n"\
"Quando usi questo metodo devi usare un valore tra \"8\" e \"320\"\n"\
"\n"\
"Per altre informazioni usa: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions ""\
"Errore: immesso un profilo e/o delle opzioni errate per questo Preset\n"\
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
"Sono stati per la maggior parte sottosposti a test e rifiniti attraverso doppi\n"\
"test di ascolto per verificare e ottenere tale obiettivo.\n"\
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
"                             Anche se questo approccio funge, non è manco\n"\
"                             un po' flessibile come VBR, e di solito non dà"\
"                             la stessa qualità del VBR a più alti bitrate.\n"\
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
"            viene leggermente più alto rispetto alla modalità normale\n"\
"            e la qualità leggermente inferiore.\n"\
"   Attenzione: nell'attuale versione l'utilizzo di Preset \"veloce\" può\n"\
"            portare un bitrate troppo alto del normale.\n"\
"\n"\
"   \"cbr\"  - se usi la modalità ABR (leggi sopra) con un certo bitrate\n"\
"            significativo come 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            puoi usare l'opzione \"cbr\" per forzare l'encoding in modalità\n"\
"            CBR al posto dello standard abr. ABR dà una più alta qualità,\n"\
"            ma CBR torna utile in quelle situazioni dove ad esempio\n"\
"            trasmettere un mp3 su internet può essere importante.\n"\
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
#define MSGTR_LameCantInit ""\
"Non posso impostare le opzioni di LAME, controlla bitrate/samplerate,\n"\
"per bitrate molto bassi (<32) servono minori samplerate (es. -srate 8000).\n"\
"Se ogni altra cosa non funziona, prova un Preset."
#define MSGTR_ConfigfileError "errore file di configurazione"
#define MSGTR_ErrorParsingCommandLine "errore leggendo la riga comando"
#define MSGTR_VideoStreamRequired "Il flusso video è obbligatorio!\n"
#define MSGTR_ForcingInputFPS "i fps saranno interpretati come %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Il formato output RAWVIDEO non supporta l'audio - lo disabilito\n"
#define MSGTR_DemuxerDoesntSupportNosound "Questo demuxer non supporta ancora -nosound.\n"
#define MSGTR_NoMatchingFilter "Non trovo il filtro/il formato ao corrispondente!\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, Manca il nome del codec!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, Non trovo l'encoder per il codec %s\n"
#define MSGTR_CouldntOpenCodec "Non posso aprire il codec %s, br=%d\n"
#define MSGTR_CantCopyAudioFormat "Il formato audio 0x%x è incompatible con '-oac copy', prova invece '-oac pcm' o usa '-fafmttag' per forzare.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metodo bitrate variabile\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
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
#define MSGTR_ParseErrorFIDAliasNotNumber "errore lettura (l'alias ID formato not è un numero?)"
#define MSGTR_DuplicateFID "ID formato duplicato"
#define MSGTR_TooManyOut "troppi out..."
#define MSGTR_InvalidCodecName "\nnome codec(%s) non valido!\n"
#define MSGTR_CodecLacksFourcc "\nil codec(%s) non ha FourCC/formato!\n"
#define MSGTR_CodecLacksDriver "\nil codec(%s) non ha un driver!\n"
#define MSGTR_CodecNeedsDLL "\nil codec(%s) abbisogna di una 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nil codec(%s) abbisogna di un 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Non riesco ad allocare memoria per il commento."
#define MSGTR_ReadingFile "Leggo %s: "
#define MSGTR_CantOpenFileError "Non posso aprire '%s': %s\n"
#define MSGTR_CodecNameNotUnique "Il nome codec '%s' non è univoco."
#define MSGTR_CodecDefinitionIncorrect "Il codec non è correttamente definito."
#define MSGTR_OutdatedCodecsConf "Il codecs.conf è troppo vecchio/incompatibile con questa versione di MPlayer!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "Memoria esaurita"
#define MSGTR_OverridingTooLowBitrate "Il bitrate specificato è troppo basso per questo clip.\n"\
"Il bitrate minimo possibile è %.0f kbps. Ignoro il valore dato dall'utente\n"

// fifo.c
#define MSGTR_CannotMakePipe "Non posso costruire una PIPE!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Trovati troppi slot vecchi salvati da lvl %d: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "L'opzione %s non può essere usata nel file di configurazione.\n"
#define MSGTR_InvalidCmdlineOption "L'opzione %s non può essere usata da riga comando.\n"
#define MSGTR_InvalidSuboption "Errore: l'opzione '%s' non ha la subopzione '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Errore: la subopzione '%s' di '%s' deve avere un parametro!\n"
#define MSGTR_MissingOptionParameter "Errore: l'opzione '%s' deve avere un parametro!\n"
#define MSGTR_OptionListHeader "\n Nome                 Tipo            Min        Max      Global  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nTotale: %d opzioni\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo CD-ROM '%s' non trovato!\n"
#define MSGTR_ErrTrackSelect "Errore nella selezione della traccia del VCD!"
#define MSGTR_ReadSTDIN "Leggo da stdin...\n"
#define MSGTR_UnableOpenURL "Impossibile aprire la URL: %s\n"
#define MSGTR_ConnToServer "Connesso al server: %s\n"
#define MSGTR_FileNotFound "File non trovato: '%s'\n"

#define MSGTR_SMBInitError "Impossibile inizializzare la libreria libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Impossibile aprire dalla rete: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer non è stato compilato con supporto di lettura da SMB.\n"

#define MSGTR_CantOpenDVD "Impossibile aprire il dispositivo DVD: %s\n"
#define MSGTR_DVDwait "Leggo la struttura del disco, per favore aspetta...\n"
#define MSGTR_DVDnumTitles "Ci sono %d titoli su questo DVD.\n"
#define MSGTR_DVDinvalidTitle "Numero del titolo del DVD non valido: %d\n"
#define MSGTR_DVDnumChapters "Ci sono %d capitoli in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidChapter "Numero del capitolo del DVD non valido: %d\n"
#define MSGTR_DVDnumAngles "Ci sono %d angolazioni in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidAngle "Numero delle angolazioni del DVD non valido: %d\n"
#define MSGTR_DVDnoIFO "Impossibile aprire il file IFO per il titolo del DVD %d.\n"
#define MSGTR_DVDnoVOBs "Impossibile aprire il VOB del titolo (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD aperto con successo.\n"

// muxer_*.c:
#define MSGTR_TooManyStreams "Troppi flussi!"
#define MSGTR_RawMuxerOnlyOneStream "Il muxer rawaudio supporta solo un flusso audio!\n"
#define MSGTR_IgnoringVideoStream "Ignoro il flusso video!\n"
#define MSGTR_UnknownStreamType "Attenzione! Tipo flusso sconosciuto: %d\n"
#define MSGTR_WarningLenIsntDivisible "Attenzione! len non è divisibile da samplesize!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Avvertimento! Intestazione del flusso audio %d ridefinito!\n"
#define MSGTR_VideoStreamRedefined "Avvertimento! Intestazione del flusso video %d ridefinito!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti audio nel buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti video nel buffer!\n"
#define MSGTR_MaybeNI "Forse stai riproducendo un flusso/file non-interleaved o il codec non funziona?\n" \
          "Per i file .AVI, prova a forzare la modalità 'non-interleaved' con l'opz. -ni.\n"
#define MSGTR_SwitchToNi "\nRilevato file .AVI con interleave errato - passo alla modalità -ni!\n"
#define MSGTR_Detected_XXX_FileFormat "Rilevato formato file %s!\n"
#define MSGTR_DetectedAudiofile "Rilevato file audio!\n"
#define MSGTR_NotSystemStream "il formato non è \'MPEG System Stream\'... (forse è \'Transport Stream\'?)\n"
#define MSGTR_InvalidMPEGES "Flusso MPEG-ES non valido??? Contatta l\'autore, può essere un baco :(\n"
#define MSGTR_FormatNotRecognized "===== Mi dispiace, questo formato file non è riconosciuto/supportato ======\n"\
				  "=== Se questo è un file AVI, ASF o MPEG, per favore contatta l\'autore! ===\n"
#define MSGTR_MissingVideoStream "Nessun flusso video trovato!\n"
#define MSGTR_MissingAudioStream "Nessun flusso audio trovato -> nessun suono\n"
#define MSGTR_MissingVideoStreamBug "Manca il flusso video!? Contatta l\'autore, può essere un baco :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: il file non contiene il flusso audio o video selezionato\n"

#define MSGTR_NI_Forced "Forzato"
#define MSGTR_NI_Detected "Rilevato"
#define MSGTR_NI_Message "%s formato file AVI NON-INTERLEAVED!\n"

#define MSGTR_UsingNINI "Uso di formato file AVI NON-INTERLEAVED corrotto.\n"
#define MSGTR_CouldntDetFNo "Impossibile determinare il numero di fotogrammi (per lo spostamento assoluto).\n"
#define MSGTR_CantSeekRawAVI "Impossibile spostarsi nei flussi .AVI grezzi. (richiesto un indice, prova con l\'opzione -idx.)\n"
#define MSGTR_CantSeekFile "Impossibile spostarsi in questo file!  \n"

#define MSGTR_EncryptedVOB "File VOB criptato! Leggi il file DOCS/HTML/en/dvd.html.\n"

#define MSGTR_MOVcomprhdr "MOV: Il supporto delle intestazioni compresse richiede ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: Avvertimento! Rilevato FOURCC variabile!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Avvertimento! troppe tracce!"
#define MSGTR_FoundAudioStream "==> Trovato flusso audio: %d\n"
#define MSGTR_FoundVideoStream "==> Trovato flusso video: %d\n"
#define MSGTR_DetectedTV "Ho trovato una TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Impossibile aprire il demuxer ogg\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: sto cercandi il flusso audio (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Impossibile aprire il flusso audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Impossibile aprire il flusso dei sottotitoli: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Errore nell'apertura del demuxer audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Errore nell'apertura del demuxer dei sottotitoli: %s\n"
#define MSGTR_TVInputNotSeekable "Impossibile spostarsi in un programma TV!\n"\
"(Probabilmente lo spostamento sarà usato per cambiare canale ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Info demuxer %s già presente!\n"
#define MSGTR_ClipInfo "Informazioni clip: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Rilevato formato NTSC 30000/1001fps, cambio framerate.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: Rilevato formato NTSC 24000/1001fps progressivo, cambio framerate.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "impossibile aprire il codec\n"
#define MSGTR_CantCloseCodec "impossibile chiudere il codec\n"

#define MSGTR_MissingDLLcodec "ERRORE: Impossibile aprire il codec DirectShow richiesto: %s\n"
#define MSGTR_ACMiniterror "Impossibile caricare/inizializz. il codec AUDIO Win32/ACM (manca il file DLL?)\n"
#define MSGTR_MissingLAVCcodec "Impossibile trovare il codec '%s' in libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF mentre cercavo la sequenza di intestazione\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Impossibile leggere la sequenza di intestazione!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Impossibile leggere l\'estensione della sequenza di intestazione!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Sequenza di intestazione non valida!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Estensione della sequenza di intestazione non valida!\n"

#define MSGTR_ShMemAllocFail "Impossibile allocare la memoria condivisa\n"
#define MSGTR_CantAllocAudioBuf "Impossibile allocare il buffer di uscita dell\'audio\n"

#define MSGTR_UnknownAudio "Formato audio sconosciuto/mancante, non uso l\'audio\n"

#define MSGTR_UsingExternalPP "[PP] Utilizzo un filtro di postprocessing esterno, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Utilizzo il postprocessing del codec, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "L'attributo video '%s' non è supportato dal vo & vd selezionati! \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Famiglia di codec video voluta [%s] (vfm=%s) indisponibile.\nAbilitala in compilazione.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Famiglia di codec audio voluta [%s] (afm=%s) indisponibile.\nAbilitala in compilazione.\n"
#define MSGTR_OpeningVideoDecoder "Apertura decoder video: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Apertura decoder audio: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s  \n"
#define MSGTR_UninitAudioStr "uninit audio: %s  \n"
#define MSGTR_VDecoderInitFailed "Inizializazione VDecoder fallita :(\n"
#define MSGTR_ADecoderInitFailed "Inizializazione ADecoder fallita :(\n"
#define MSGTR_ADecoderPreinitFailed "Preinizializazione ADecoder fallita :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alloco %d byte per il buffer di input\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alloco %d + %d = %d byte per il buffer di output\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurazione del supporto per lirc...\n"
#define MSGTR_LIRCdisabled "Non potrai usare il tuo telecomando\n"
#define MSGTR_LIRCopenfailed "Apertura del supporto per lirc fallita!\n"
#define MSGTR_LIRCcfgerr "Fallimento nella lettura del file di configurazione di LIRC %s!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Impossibile trovare il filtro video '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Impossibile aprire il filtro video '%s'\n"
#define MSGTR_OpeningVideoFilter "Apertura filtro filter: "
#define MSGTR_CannotFindColorspace "Impossibile trovare un colorspace in comune, anche inserendo 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Il codec non ha impostato sh->disp_w and sh->disp_h, tento di risolvere.!\n"
#define MSGTR_VoConfigRequest "VDec: configurazione richiesta dal vo - %d x %d (csp preferito: %s)\n"
#define MSGTR_CouldNotFindColorspace "Impossibile trovare un colorspace adatto - riprovo con -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect è %.2f:1 - riscalo per ottenere un rapporto corretto.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect non definito - nessuna scalatura.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Devi installare o aggiornare i codec binari.\nVai a http://mplayerhq.hu/homepage/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow inizializzato correttamente.\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO inizializzato correttamente.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Impossibile inviare l'evento schermo pieno EWMH!\n"

#define MSGTR_InsertingAfVolume "[Mixer] Nessun mixer hardware, filtro volume inserito automaticamente.\n"
#define MSGTR_NoVolume "[Mixer] Nessuna regolazione di volume disponibile.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Informazioni su"
#define MSGTR_FileSelect "Seleziona il file..."
#define MSGTR_SubtitleSelect "Seleziona il sottotitolo..."
#define MSGTR_OtherSelect "Seleziona..."
#define MSGTR_AudioFileSelect "Seleziona canale audio esterno..."
#define MSGTR_FontSelect "Seleziona il carattere..."
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizzatore"			 
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
#define MSGTR_NEMDB "Mi dispiace, non c'è sufficiente memoria per il buffer di disegno."
#define MSGTR_NEMFMR "Mi dispiace, non c'è sufficiente memoria per visualizzare il menu."
#define MSGTR_IDFGCVD "Mi dispiace, non ho trovato un driver di output video compatibile con la GUI."
#define MSGTR_NEEDLAVCFAME "Mi dispiace, non puoi riprodurre file non-MPEG con il tuo dispositivo DXR3/H+\nsenza ricodificarli.\nAbilita lavc o fame nella finestra di configurazione DXR3/H+."
#define MSGTR_UNKNOWNWINDOWTYPE "Trovato tipo finestra sconosciuto..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] errore nel file di configurazione della skin alla riga %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] avvertimento nel file di configurazione della skin alla riga %d:\nwidget trovato ma non trovata prima la \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] avvertimento nel file di configurazione della skin alla riga %d:\nwidget trovato ma non trovata prima la \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] avvertimento nel file di configurazione della skin alla riga %d:\nquesta sottosezione non è supportata dal widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] file ( %s ) non trovato.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] file ( %s ) non leggibile.\n"
#define MSGTR_SKIN_BITMAP_16bit  "bitmap con profondità di 16 bit o inferiore non supportata (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file non trovato (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP, errore di lettura (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA, errore di lettura (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG, errore di lettura (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed TGA non supportato (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "tipo di file sconosciuto (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "errore nella conversione da 24 bit a 32 bit (%s)\n"
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

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Informazione su MPlayer"
#define MSGTR_MENU_Open "Apri..."
#define MSGTR_MENU_PlayFile "Riproduci il file..."
#define MSGTR_MENU_PlayVCD "Riproduci il VCD..."
#define MSGTR_MENU_PlayDVD "Riproduci il DVD..."
#define MSGTR_MENU_PlayURL "Riproduci la URL..."
#define MSGTR_MENU_LoadSubtitle "Carica i sottotitoli..."
#define MSGTR_MENU_DropSubtitle "Elimina i sototitoli..."
#define MSGTR_MENU_LoadExternAudioFile "Carica file audio esterni..."
#define MSGTR_MENU_Playing "Riproduzione"
#define MSGTR_MENU_Play "Riproduci"
#define MSGTR_MENU_Pause "Pausa"
#define MSGTR_MENU_Stop "Interrompi"
#define MSGTR_MENU_NextStream "Stream successivo"
#define MSGTR_MENU_PrevStream "Stream precedente"
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
#define MSGTR_MENU_AudioLanguages "Lingua dell\'audio"
#define MSGTR_MENU_SubtitleLanguages "Lingua dei sottotitoli"
#define MSGTR_MENU_SkinBrowser "Ricerca skin"
#define MSGTR_MENU_Exit "Uscita..."
#define MSGTR_MENU_Mute "Muto"
#define MSGTR_MENU_Original "Originale"
#define MSGTR_MENU_AspectRatio "Aspetto"
#define MSGTR_MENU_AudioTrack "Traccia audio"
#define MSGTR_MENU_Track "Traccia %d"
#define MSGTR_MENU_VideoTrack "Traccia video"

// --- equalizer
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
#define MSGTR_PREFERENCES_Misc "Varie"
#define MSGTR_PREFERENCES_None "Nessuno"
#define MSGTR_PREFERENCES_DriverDefault "Driver predefinito"
#define MSGTR_PREFERENCES_AvailableDrivers "Driver disponibili:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Non riprodurre l'audio"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizza l'audio"
#define MSGTR_PREFERENCES_EnEqualizer "Abilita l'equalizzatore"
#define MSGTR_PREFERENCES_SoftwareMixer "Abilita Mixer Software"
#define MSGTR_PREFERENCES_ExtraStereo "Abilita l'extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficiente:"
#define MSGTR_PREFERENCES_AudioDelay "Ritatdo audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Abilita il doppio buffering"
#define MSGTR_PREFERENCES_DirectRender "Abilita il direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Abilita lo scarto dei frame"
#define MSGTR_PREFERENCES_HFrameDrop "Abilita lo scarto HARD (forte) dei frame (pericoloso)"
#define MSGTR_PREFERENCES_Flip "Ribalta l'immagine sottosopra"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timer e indicatori"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, percentuale e tempo totale"
#define MSGTR_PREFERENCES_OSDProgress "Solo progressbars"
#define MSGTR_PREFERENCES_Subtitle "Sottotitolo:"
#define MSGTR_PREFERENCES_SUB_Delay "Ritardo: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Posizione: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Disattiva il caricamento automatico dei sottotitoli"
#define MSGTR_PREFERENCES_SUB_Unicode "Sottotitoli unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Converti i sottotitoli nel formato sottotitolo di MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Converti i sottotitoli nel formato SubViewer (SRT) basato sul tempo"
#define MSGTR_PREFERENCES_SUB_Overlap "Attiva/Disattiva sovrapposizione sottotitoli"
#define MSGTR_PREFERENCES_Font "Carattere:"
#define MSGTR_PREFERENCES_Codecs "Codec e demuxer"
#define MSGTR_PREFERENCES_FontFactor "Font factor:"
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
#define MSGTR_PREFERENCES_Audio_Device "Dispositivo:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Canale mixer:"
#define MSGTR_PREFERENCES_Message "Ricorda che devi riavviare la riproduzione affinché alcune opzioni abbiano effetto!"
#define MSGTR_PREFERENCES_DXR3_VENC "Video encoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Usa LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Usa FAME"
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
#define MSGTR_PREFERENCES_FontNoAutoScale "No autoscale"
#define MSGTR_PREFERENCES_FontPropWidth "Proporzionale alla larghezza del filmato"
#define MSGTR_PREFERENCES_FontPropHeight "Proporzionale all'altezza del filmato"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporzionale alla diagonale del filmato"
#define MSGTR_PREFERENCES_FontEncoding "Codifica:"
#define MSGTR_PREFERENCES_FontBlur "Blur:"
#define MSGTR_PREFERENCES_FontOutLine "Outline:"
#define MSGTR_PREFERENCES_FontTextScale "Text scale:"
#define MSGTR_PREFERENCES_FontOSDScale "Scala OSD:"
#define MSGTR_PREFERENCES_SubtitleOSD "Sottotitoli & OSD"
#define MSGTR_PREFERENCES_Cache "Cache on/off"
#define MSGTR_PREFERENCES_LoadFullscreen "Avvia a pieno schermo"
#define MSGTR_PREFERENCES_CacheSize "Dimensione cache: "
#define MSGTR_PREFERENCES_SaveWinPos "Salva la posizione della finestra"
#define MSGTR_PREFERENCES_XSCREENSAVER "Arresta XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Attiva playbar"
#define MSGTR_PREFERENCES_AutoSync "AutoSync on/off"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "Dispositivo CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Dispositivo DVD:"
#define MSGTR_PREFERENCES_FPS "FPS del filmato:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Mostra la finestra video anche quando non è attiva"

#define MSGTR_ABOUT_UHU "Lo sviluppo della GUI è sponsorizzato da UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   Team sviluppo MPlayer:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Altri programmatori:\n"
#define MSGTR_ABOUT_MainTesters "   Tester principali:\n"

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
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Spiacente, non abbastanza memoria per il buffer di disegno.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS non disponibile?\n"
#define MSGTR_WS_DpmsNotEnabled "Non posso abilitare DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Questo non sembra essere un file...\n"
#define MSGTR_WS_DDNothing "D&D: Nessun valore di ritorno!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Il dispositivo di uscita video_out scelto è incompatibile con questo codec.\n"
#define MSGTR_VO_GenericError "E' accaduto questo errore"
#define MSGTR_VO_UnableToAccess "Impossibile accedere a"
#define MSGTR_VO_ExistsButNoDirectory "già esiste, ma non è una directory."
#define MSGTR_VO_DirExistsButNotWritable "La directory di output esiste già, ma non è scrivibile."
#define MSGTR_VO_DirExistsAndIsWritable "La directory di output esiste già ed è scrivibile."
#define MSGTR_VO_CantCreateDirectory "Non posso creare la directory di output."
#define MSGTR_VO_CantCreateFile "Non posso creare il file di output."
#define MSGTR_VO_DirectoryCreateSuccess "Directory di output creata con successo."
#define MSGTR_VO_ParsingSuboptions "Leggo subopzioni."
#define MSGTR_VO_SuboptionsParsedOK "Lettura subopzioni OK."
#define MSGTR_VO_ValueOutOfRange "Valore fuori gamma"
#define MSGTR_VO_NoValueSpecified "Nessun valore specificato."
#define MSGTR_VO_UnknownSuboptions "Subopzione/i sconosciuta/e"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nEcco le subopzioni per l'aalib vo_aa:\n"
#define MSGTR_VO_AA_AdditionalOptions "Le opzioni addizionali di vo_aa:\n" \
"  help        mostra questo messaggio\n" \
"  osdcolor    imposta colore osd\n" \
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
// TODO: chrominance = ?
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "L'input non è RGB, non posso separare la chrominance per campi!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "La larghezza immagine dev'essere divisibile per 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Non c'è abbastanza memoria per allocare il framebuffer RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Non posso allocare memoria o spazio per scrivere \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Errore di scrittura dell'immagine in uscita!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Subdispositivo sconosciuto: %s"
// TODO: top-field/bottom-field first = ?
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Uso modalità di uscita interlacciata, top-field first."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Uso modalità di uscita interlacciata, bottom-field first."
// TODO: progressive frame = ?
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Uso la modalità frame progressivo (default)."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Il driver di output video pgm è stato sostituito con -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Il driver di output video md5 è stato sostituito con -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: i moduli alsa9/alsa1x sono stati rimossi, ora usa -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Non posso aprire il dispositivo mixer %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Il mixer scheda audio non ha il canale '%s' uso default.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Non posso aprire il dispositivo audio %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Non riesco a bloccare il dispositivo: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Non posso impostare il device audio %s a %s output, provo %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Fallimento nell'impostazione audio a %d canali.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: il driver non supporta SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Il tuo driver audio NON supporta select()  ***\n Ricompila MPlayer con #undef HAVE_AUDIO_SELECT in config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nErrore fatale: *** NON POSSO RIAPRIRE / RESETTARE IL DEVICE AUDIO *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Connesso al server del suono.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Non posso apire un flusso.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Flusso aperto.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] dimensione buffer: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Impostazione del volume a %d fallita.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz non supportati, prova \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound fallito: %s\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] fallimento nell'aprire il flusso di riproduzione esd: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Impostazione mixer DVB fallita: %s\n" 
// TODO: resample = ?
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz non supportati, prova con resample...\n"

// ao_pcm.c
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: un dump più veloce si ottiene con -vc dummy -vo null\nPCM: Info: per scrivere files WAVE usa -waveheader (default).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Non posso aprire %s in scrittura!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] Uso il driver audio %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Formato audio non supportato: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Inizializzazione del SDL fallita: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Non posso aprire l'audio: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: dispositivo non valido.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams fallito: %s\nNon posso impostare il samplerate voluto.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE non è stato accettato dalla risorsa.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams fallito: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: il samplerate ora è %lf (la frequenza voluta è %lf)\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Non posso apire il canale audio: %s\n"

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
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: errore informazioni pcm: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d scheda/e audio travata/e, uso: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: errore informazioni canale pcm: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: errore impostazione parametri: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: errore nell'impostazione canale: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: preparazione del canale: %s\n"
// TODO: drain, flush = ?
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: errore drain riproduzione: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: errore flush riproduzione: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: errore chiusura pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: errore drain riproduzione: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: errore flush riproduzione: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: errore preparazione canale: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: errore drain riproduzione: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: errore flush riproduzione: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: errore preparazione canale: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: ritardo alsa, reimposto il flusso.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: errore preparazione riproduzione: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: errore di scrittura dopo reset: %s - mi arrendo.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: errore di output: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] plugin non valido: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "etichette disponibili in"
#define MSGTR_AF_LADSPA_WarnNoInputs "WARNING! Questo plugin LADSPA non ha entrate audio.\n  Il segnale audio in entrata verrà perso."
#define MSGTR_AF_LADSPA_ErrMultiChannel "I plugin multi-canale (>2) non sono supportati (finora).\n  Usare solo i plugin mono e stereo."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Questo plugin LADSPA non ha uscite audio."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Il numero delle entrate e uscite audio del plugin LADSPA differiscono."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "fallimento nel caricare"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Non trovo la funzione ladspa_descriptor() nella libreria indicata."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Non trovo l'etichetta nella libreria plugin."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Nessuna subopzione  indicata"
#define MSGTR_AF_LADSPA_ErrNoLibFile "Nessuna libreria indicata"
#define MSGTR_AF_LADSPA_ErrNoLabel "Nessuna etichetta di filtro indicata"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Non sono stati indicati abbastaza controlli sulla riga comando"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Il controllo di input #%d è sotto il limite inferiore di %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Il controllo di input #%d è sopra al limite superiore di %0.4f.\n"

