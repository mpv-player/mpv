// Last sync on 2006-11-09 with help_mp-en.h 20191
// Additionnal updates, fixes and translations by P Lombard <p_l@gmx.fr>
//   G Pelletier <pellgill@gmail.com> & A Coutherez <newt@neopulsar.org>
//   C Dumez-Viou <viou@obs-nancay.fr>
// Updates & fixes by pl <p_l@gmx.fr> & n.le gaillart <n@tourmentine.com>
// Original translation by Firebird <firebird@chez.com>

// ========================= Aide MPlayer ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Utilisation :      mplayer [options] [url|répertoire/]fichier\n"
"\n"
"Options de base :  (liste complète dans la page de man)\n"
" -vo <pil[:pér]>  pilote et périph. vidéo de sortie ('-vo help' pour liste)\n"
" -ao <pil[:pér]>  pilote et périph. audio de sortie ('-ao help' pour liste)\n"
#ifdef CONFIG_VCD
" vcd://<n°piste>  lit piste (S)VCD (Super Video CD) (périf. brut, non-monté)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<n°titre>  lit titre DVD du périf. plutôt que d'un fichier\n"
" -alang/-slang    langue audio/sous-titres du DVD (code pays 2 lettres)\n"
#endif
" -ss <pos>        démarre lecture à 'pos' (temps en secondes ou hh:mm:ss)\n"
" -nosound         ne joue aucun son\n"
" -fs              plein-écran (ou -vm, -zoom, détails dans page man)\n"
" -x <x> -y <y>    résolution de l'affichage (à utiliser avec -vm ou -zoom)\n"
" -sub <fich>      fichier sous-titres à utiliser (cf. -subfps, -subdelay)\n"
" -playlist <fich> fichier des titres audio à lire\n"
" -vid x -aid y    spécifie les flux vidéo (x) et audio (y) à lire\n"
" -fps x -srate y  change fréquences vidéo (x fps) et audio (y Hz)\n"
" -pp <qualité>    active le filtre de post-traitement (détails page man)\n"
" -framedrop       active saut d'images (pour machines lentes)\n"
"\n"
"Fonctions au clavier: (liste complète dans page man, voir aussi input.conf)\n"
" <- ou ->         arrière/avant 10 secondes\n"
" haut ou bas      arrière/avant 1 minute\n"
" PgUp ou PgDown   arrière/avant 10 minutes\n"
" < ou >           fichier précédent/suivant dans liste audio à lire\n"
" p ou ESPACE      pause film (presser n'importe quelle touche pour continuer)\n"
" q ou ESC         arrête la lecture et quitte le programme\n"
" + ou -           ajuste délai audio : +/- 0.1 seconde\n"
" o                cycle mode OSD: aucun/barre recherche/barre rech. + temps\n"
" * ou /           augmente/diminue le volume PCM\n"
" x ou z           ajuste délai des sous-titres : +/- 0.1 seconde\n"
" r ou t           ajuste position sous-titres : +haut/+bas, cf. -vf expand\n"
"\n"
" * * * VOIR PAGE MAN POUR DÉTAILS, AUTRES OPTIONS (AVANCÉES) ET TOUCHES * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Échantillons ce format demandés pour améliorer support. Contacter developpeurs.\n"

// ========================= Messages MPlayer ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nSortie...\n"
#define MSGTR_ExitingHow "\nSortie... (%s)\n"
#define MSGTR_Exit_quit "Fin"
#define MSGTR_Exit_eof "Fin du fichier"
#define MSGTR_Exit_error "Erreur fatale"
#define MSGTR_IntBySignal "\nMPlayer interrompu par le signal %d dans le module : %s\n"
#define MSGTR_NoHomeDir "Impossible de trouver le répertoire HOME.\n"
#define MSGTR_GetpathProblem "Problème get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Création du fichier config : %s\n"
#define MSGTR_BuiltinCodecsConf "Utilisation du codecs.conf intégré par défaut\n"
#define MSGTR_CantLoadFont "Ne peut charger la police : %s\n"
#define MSGTR_CantLoadSub "Ne peut charger les sous-titres : %s\n"
#define MSGTR_DumpSelectedStreamMissing "Vidage de la mémoire (dump) : FATAL : flux sélectionné manquant !\n"
#define MSGTR_CantOpenDumpfile "Impossible d'ouvrir le fichier pour le vidage de la mémoire (dump).\n"
#define MSGTR_CoreDumped "Vidage de la mémoire du noyeau (core dump) effectué ;)\n"
#define MSGTR_FPSnotspecified "FPS non spécifié dans l'entête ou invalide ! Utilisez l'option -fps.\n"
#define MSGTR_TryForceAudioFmtStr "Tente de forcer la famille de codecs audio %s ...\n"
#define MSGTR_CantFindAudioCodec "Ne peut trouver de codec pour le format audio 0x%X.\n"
#define MSGTR_RTFMCodecs "Veuillez lire DOCS/HTML/fr/codecs.html !\n"
#define MSGTR_TryForceVideoFmtStr "Tente de forcer la famille de codecs vidéo %s  ...\n"
#define MSGTR_CantFindVideoCodec "Ne peut trouver codec pour format -vo sélectionné et vidéo 0x%X.\n"
#define MSGTR_CannotInitVO "FATAL : Ne peut initialiser le pilote vidéo.\n"
#define MSGTR_CannotInitAO "Ne peut ouvrir/initialiser le périphérique audio -> pas de son.\n"
#define MSGTR_StartPlaying "Démarre la lecture...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         *************************************************************\n"\
"         **** Votre système est trop LENT pour jouer ce fichier ! ****\n"\
"         *************************************************************\n\n"\
"Raisons possibles, problèmes, solutions :\n"\
"- Le plus courant : pilote _audio_ corrompu/bogué\n"\
"  - Essayez -ao sdl ou l'émulation OSS d'ALSA.\n"\
"  - Essayez différentes valeurs pour -autosync, 30 est un bon début.\n"\
"- Sortie vidéo lente\n"\
"  - Essayez avec un pilote -vo différent (-vo help pour la liste) ou\n"\
"    essayez avec -framedrop !\n"\
"- CPU lent\n"\
"  - N'essayez pas de lire de gros DVD/DivX sur un CPU lent !\n"\
"    Essayez une des options -lavdopts,\n"\
"    e.g. -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
"- Fichier corrompu\n"\
"  - Essayez différentes combinaisons de -nobps -ni -forceidx -mc 0.\n"\
"- Pour jouer depuis un média lent (NFS/SMB, DVD, VCD, etc.)\n"\
"  - Essayez -cache 8192\n"\
"- Utilisez-vous -cache avec un fichier AVI non multiplexé ? \n"\
"  - Essayez avec -nocache\n"\
"Lisez DOCS/HTML/fr/video.html pour les astuces de réglage/accélération.\n"\
"Si rien de tout cela ne vous aide, lisez DOCS/HTML/fr/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer a été compilé SANS support GUI.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI a besoin de X11.\n"
#define MSGTR_Playing "Lecture de %s\n"
#define MSGTR_NoSound "Audio : pas de son\n"
#define MSGTR_FPSforced "FPS forcé à %5.3f  (ftime : %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Compilé avec détection du CPU à l'exécution."
#define MSGTR_CompiledWithCPUExtensions "Compilé pour CPU x86 avec les extensions:"
#define MSGTR_AvailableVideoOutputDrivers "Pilotes de sortie vidéo disponibles :\n"
#define MSGTR_AvailableAudioOutputDrivers "Pilotes de sortie audio disponibles :\n"
#define MSGTR_AvailableAudioCodecs "Codecs audio disponibles :\n"
#define MSGTR_AvailableVideoCodecs "Codecs vidéo disponibles :\n"
#define MSGTR_AvailableAudioFm "Familles/pilotes de codecs audio disponibles (inclus à la compilation) :\n"
#define MSGTR_AvailableVideoFm "Familles/pilotes de codecs vidéo disponibles (inclus à la compilation) :\n"
#define MSGTR_AvailableFsType "Modes de changement de couches plein écran disponibles :\n"
#define MSGTR_UsingRTCTiming "Utilisation de la synchronisation matérielle par RTC (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Vidéo : impossible de lire les propriétés\n"
#define MSGTR_NoStreamFound "Aucun flux trouvé.\n"
#define MSGTR_ErrorInitializingVODevice "Erreur à l'ouverture/initialisation de la sortie vidéo choisie (-vo).\n"
#define MSGTR_ForcedVideoCodec "Codec vidéo forcé : %s\n"
#define MSGTR_ForcedAudioCodec "Codec audio forcé : %s\n"
#define MSGTR_Video_NoVideo "Vidéo : pas de vidéo\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL : impossible d'initialiser filtres vidéo (-vf) ou sortie vidéo (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUSE  =====\r" // pas plus de 23 caractères (ligne pour les fichiers audio)
#define MSGTR_PlaylistLoadUnable "\nImpossible de charger la liste de lecture %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer a planté à cause d'une 'Instruction Illégale'.\n"\
"  Il y a peut-être un bogue dans notre nouveau code de détection CPU...\n"\
"  Veuillez lire DOCS/HTML/fr/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer a planté à cause d'une 'Instruction Illégale'.\n"\
"  Cela se produit généralement quand vous le lancez sur un CPU différent\n"\
"  de celui pour lequel il a été compilé/optimisé.\n Vérifiez !\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer a planté à cause d'une mauvaise utilisation de CPU/FPU/RAM.\n"\
"  Recompilez MPlayer avec --enable-debug et faites un backtrace 'gdb' et\n"\
"  désassemblage. Détails : DOCS/HTML/fr/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer a planté. Cela n'aurait pas dû arriver.\n"\
"  Peut-être un bogue dans code de MPlayer _ou_ dans vos pilotes _ou_ dans votre\n"\
"  version de gcc. C'est la faute de MPlayer ? Lire DOCS/HTML/fr/bugreports.html\n"\
"  et suivre les instructions. Nous pourrons et voudrons vous aider si vous\n"\
"  fournissiez ces informations en rapportant un bogue possible.\n"
#define MSGTR_LoadingConfig "Chargement du fichier de configuration '%s'\n"
#define MSGTR_LoadingProtocolProfile "Chargement du profil de protocol '%s'\n"
#define MSGTR_LoadingExtensionProfile "Chargement du profil d'extension '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB : fichier sous-titres ajouté (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB : fichier sous-titres enlevé (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Erreur d'ouverture du fichier [%s] en écriture !\n"
#define MSGTR_CommandLine "Ligne de commande :"
#define MSGTR_RTCDeviceNotOpenable "Échec à l'ouverture de %s : %s (devrait être lisible par l'utilisateur.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Erreur init RTC Linux dans ioctl (rtc_irqp_set %lu) : %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Essayer ajout \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" au script de démarrage de votre système.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Erreur init RTC Linux dans ioctl (rtc_pie_on) : %s\n"
#define MSGTR_UsingTimingType "Utilisation de minuterie %s.\n"
#define MSGTR_NoIdleAndGui "L'option -idle ne peut être utilisée avec GMPlayer.\n"
#define MSGTR_MenuInitialized "Menu initialisé : %s\n"
#define MSGTR_MenuInitFailed "Échec d'initialisation du menu.\n"
#define MSGTR_Getch2InitializedTwice "ATTENTION : getch2_init appelé deux fois !\n"
#define MSGTR_DumpstreamFdUnavailable "Impossible de vider ce flux - Aucun descripteur de fichier disponible.\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Impossible d'ouvrir filtre vidéo libmenu avec menu root %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Erreur de pré-initialisation de la chaîne de filtres audio !\n"
#define MSGTR_LinuxRTCReadError "Erreur de lecture horloge temps réel (RTC) Linux : %s\n"
#define MSGTR_SoftsleepUnderflow "Attention ! Soupassement sommeil léger (time_frame négatif)!\n"
#define MSGTR_DvdnavNullEvent "Événement DVDNAV NUL ?!\n"
#define MSGTR_DvdnavHighlightEventBroken "Événement DVDNAV : Événement surbrillance rompu\n"
#define MSGTR_DvdnavEvent "Événement DVDNAV : %s\n"
#define MSGTR_DvdnavHighlightHide "Événement DVDNAV : Cache surbrillance\n"
#define MSGTR_DvdnavStillFrame "#################################### Événement DVDNAV : Image fixe : %d sec(s)\n"
#define MSGTR_DvdnavNavStop "Événement DVDNAV : Arret de navigation \n"
#define MSGTR_DvdnavNavNOP "Événement DVDNAV : Pas d'opération (NOP) navigation \n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "Événement DVDNAV : Changement flux SPU nav : phys : %d/%d/%d log : %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "Événement DVDNAV : Changement de flux de navigation SPU : phys:  %d logique : %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "Événement DVDNAV : Changement de flux de navigation Audio : phys : %d logique : %d\n"
#define MSGTR_DvdnavNavVTSChange "Événement DVDNAV : Changement de navigation VTS\n"
#define MSGTR_DvdnavNavCellChange "Événement DVDNAV : Changement de cellule de navigation\n"
#define MSGTR_DvdnavNavSpuClutChange "Événement DVDNAV : Changement de navigation SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "Événement DVDNAV : Cherche navigation faite\n"
#define MSGTR_MenuCall "Appel menu\n"

#define MSGTR_EdlOutOfMem "Impossible d'allouer assez de mémoire pour contenir les données EDL.\n"
#define MSGTR_EdlRecordsNo "Lu %d actions EDL.\n"
#define MSGTR_EdlQueueEmpty "Aucune action EDL à gérer.\n"
#define MSGTR_EdlCantOpenForWrite "Impossible d'ouvrir fichier EDL [%s] en écriture.\n"
#define MSGTR_EdlCantOpenForRead "Impossible d'ouvrir fichier EDL [%s] en lecture.\n"
#define MSGTR_EdlNOsh_video "Impossible d'utiliser EDL sans video, désactive.\n"
#define MSGTR_EdlNOValidLine "Ligne EDL invalide : %s\n"
#define MSGTR_EdlBadlyFormattedLine "Ligne EDL mal formatée [%d] Rejet.\n"
#define MSGTR_EdlBadLineOverlap "Dernière position d'arret : [%f] ; départ suivant : "\
"[%f]. Entrées doivent être en ordre chrono, ne peuvent se chevaucher. Rejet.\n"
#define MSGTR_EdlBadLineBadStop "Temps d'arrêt doit être après temps de départ.\n"
#define MSGTR_EdloutBadStop "Saut EDL annulé, dernier début > arrêt\n"
#define MSGTR_EdloutStartSkip "EDL saute le début, presse 'i' encore une fois pour fin du bloc.\n"
#define MSGTR_EdloutEndSkip "EDL saute la fin, ligne écrite.\n"
#define MSGTR_MPEndposNoSizeBased "Option -endpos dans MPlayer ne supporte pas encore les unités de taille.\n"


// mplayer.c OSD

#define MSGTR_OSDenabled "activé"
#define MSGTR_OSDdisabled "désactivé"
#define MSGTR_OSDAudio "Audio : %s"
#define MSGTR_OSDVideo "Vidéo : %s"
#define MSGTR_OSDChannel "Canal : %s"
#define MSGTR_OSDSubDelay "Décalage : %d ms"
#define MSGTR_OSDSpeed "Vitesse : x %6.2f"
#define MSGTR_OSDosd "OSD : %s"
#define MSGTR_OSDChapter "Chapitre : (%d) %s"
#define MSGTR_OSDAngle "Angle: %d/%d"

// property values
#define MSGTR_Enabled "activé"
#define MSGTR_EnabledEdl "activé (edl)"
#define MSGTR_Disabled "désactivé"
#define MSGTR_HardFrameDrop "dur"
#define MSGTR_Unknown "inconnu"
#define MSGTR_Bottom "bas"
#define MSGTR_Center "centre"
#define MSGTR_Top "haut"
#define MSGTR_SubSourceFile "fichier"
#define MSGTR_SubSourceVobsub "vobsub"
#define MSGTR_SubSourceDemux "inclus"

// osd bar names
#define MSGTR_Volume "Volume"
#define MSGTR_Panscan "Recadrage"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "Brillance"
#define MSGTR_Contrast "Contraste"
#define MSGTR_Saturation "Saturation"
#define MSGTR_Hue "Tonalité"
#define MSGTR_Balance "Balance"

// property state
#define MSGTR_LoopStatus "Boucle: %s"
#define MSGTR_MuteStatus "Silence : %s"
#define MSGTR_AVDelayStatus "Delai A-V : %s"
#define MSGTR_OnTopStatus "Reste au dessus : %s"
#define MSGTR_RootwinStatus "Fenêtre racine : %s"
#define MSGTR_BorderStatus "Bordure : %s"
#define MSGTR_FramedroppingStatus "Saut d'images : %s"
#define MSGTR_VSyncStatus "Sync verticale : %s"
#define MSGTR_SubSelectStatus "Sous-titres : %s"
#define MSGTR_SubSourceStatus "Source des sous-titres : %s"
#define MSGTR_SubPosStatus "Position des sous-titres : %s/100"
#define MSGTR_SubAlignStatus "Alignement des sous-titres : %s"
#define MSGTR_SubDelayStatus "Décalage des sous-titres : %s"
#define MSGTR_SubScale "Échelle des sous-titres : %s"
#define MSGTR_SubVisibleStatus "Sous-titres : %s"

// mencoder.c

#define MSGTR_MissingFilename "\nNom de fichier manquant.\n\n"
#define MSGTR_CannotOpenFile_Device "Impossible d'ouvrir le fichier/périph.\n"
#define MSGTR_CannotOpenDemuxer "Impossible d'ouvrir le démuxeur.\n"
#define MSGTR_NoAudioEncoderSelected "\nAucun encodeur audio (-oac) choisi ! Choisissez-en un (voir aide -oac) ou -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nAucun encodeur vidéo (-ovc) choisi ! Choisissez-en un (voir l'aide pour -ovc).\n"
#define MSGTR_CannotOpenOutputFile "Impossible d'ouvrir le fichier de sortie '%s'\n"
#define MSGTR_EncoderOpenFailed "Impossible d'ouvrir l'encodeur\n"
#define MSGTR_MencoderWrongFormatAVI "\nATTENTION : LE FORMAT DU FICHIER DE SORTIE EST _AVI_. Voir '-of help'.\n"
#define MSGTR_MencoderWrongFormatMPG "\nATTENTION : LE FICHIER DU FICHIER DE SORTIE EST _MPEG_. Voir '-of help'.\n"
#define MSGTR_MissingOutputFilename "Aucun fichier de sortie spécifié, veuillez voir l'option -o"
#define MSGTR_ForcingOutputFourcc "Code fourcc de sortie forcé à %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "Forçage du tag du format audio de sortie à 0x%x\n"
#define MSGTR_DuplicateFrames "\n%d image(s) répétée(s) !\n"
#define MSGTR_SkipFrame "\nImage sautée !\n"
#define MSGTR_ResolutionDoesntMatch "\nLe nouveau fichier vidéo a une résolution ou un espace colorimétrique différent du précédent.\n"
#define MSGTR_FrameCopyFileMismatch "\nTous fichiers vidéo doivent utiliser mêmes fps, résolution, codec pour copie -ovc.\n"
#define MSGTR_AudioCopyFileMismatch "\nTous fichiers audio doivent utiliser mêmes codec et format pour copie -oac.\n"
#define MSGTR_NoAudioFileMismatch "\nNe peut mélanger fichiers vidéo seul et fichiers vidéo/audio. -nosound?\n"
#define MSGTR_NoSpeedWithFrameCopy "ATTENTION : -speed peut ne pas fonctionner correctement avec -oac copy !\n"\
"Votre encodage pourrait être brisé!\n"
#define MSGTR_ErrorWritingFile "%s : Erreur durant l'écriture du fichier.\n"
#define MSGTR_FlushingVideoFrames "\nAbandonne des trames vidéo.\n"
#define MSGTR_FiltersHaveNotBeenConfiguredEmptyFile "Les filtres n'ont pas été configurés! Fichier vide?\n"
#define MSGTR_RecommendedVideoBitrate "Débit binaire (bitrate) vidéo recommandé pour le CD %s : %d\n"
#define MSGTR_VideoStreamResult "\nFlux vidéo : %8.3f kbit/s  (%d B/s)  taille : %"PRIu64" octets  %5.3f secs  %d images\n"
#define MSGTR_AudioStreamResult "\nFlux audio : %8.3f kbit/s  (%d B/s)  taille : %"PRIu64" octets  %5.3f secs\n"
#define MSGTR_EdlSkipStartEndCurrent "Saut EDL : Début: %.2f  Fin: %.2f   Courant: V: %.2f  A: %.2f     \r"
#define MSGTR_OpenedStream "succès : format : %d  data : 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "codec vidéo : copie de trame (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "codec audio : copie img (format=%x canaux=%d taux=%d bits=%d B/s=%d échant-%d)\n"
#define MSGTR_CBRPCMAudioSelected "Audio CBR PCM selectionné\n"
#define MSGTR_MP3AudioSelected "Audio MP3 sélectionné\n"
#define MSGTR_CannotAllocateBytes "N'a pu allouer %d octets\n"
#define MSGTR_SettingAudioDelay "Réglage du délai audio à %5.3fs\n"
#define MSGTR_SettingVideoDelay "Réglage du délai vidéo à %5.3fs\n"
#define MSGTR_SettingAudioInputGain "Réglage du gain audio en entrée à %f\n"
#define MSGTR_LamePresetEquals "\npré-réglages=%s\n\n"
#define MSGTR_LimitingAudioPreload "Limitation du préchargement audio à 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Augmentation de la densité audio à 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Forçage du pré-chargement audio à 0 et de la correction max des pts à 0\n"
#define MSGTR_CBRAudioByterate "\n\nAudio CBR : %d octets/s, %d octets/bloc\n"
#define MSGTR_LameVersion "LAME version %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Erreur : le bitrate spécifié est hors de l'intervalle valide pour ce pré-réglage\n"\
"\n"\
"Lorsque vous utilisez ce mode, la valeur doit être entre \"8\" et \"320\"\n"\
"\n"\
"Pour plus d'information, essayez : \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Erreur : vous n'avez pas entré de profil valide et/ou d'option avec preset (pré-réglage)\n"\
"\n"\
"Les profils disponibles sont :\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Mode ABR) - C'est le mode par défaut. Pour l'utiliser,\n"\
"                      il suffit de préciser un bitrate. Par exemple :\n"\
"                      \"preset=185\" active ce pré-réglage\n"\
"                      et utilise un bitrate moyen de 185kbps.\n"\
"\n"\
"    Quelques exemples :\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" ou \"-lameopts  cbr:preset=192       \"\n"\
" ou \"-lameopts      preset=172       \"\n"\
" ou \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Pour plus d'informations, essayez : \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Les pré-réglages ont été conçus pour offrir la plus haute qualité possible.\n"\
"\n"\
"Ces réglages ont été optimisés par le biais de double écoute en aveugle\n"\
"pour vérifier qu'ils atteignaient leurs objectifs.\n"\
"\n"\
"Ils sont continuellement mis à jour pour tirer partie des derniers développements\n"\
"et offrent donc par conséquent la meilleure qualité possible avec LAME.\n"\
"\n"\
"Pour activer ces pré-réglages :\n"\
"\n"\
"   Pour les modes VBR (en géneral, la plus haute qualité):\n"\
"\n"\
"     \"preset=standard\" Ce mode devrait être transparent pour la plupart\n"\
"                             des gens, sur la plupart des musiques. Sa\n"\
"                             qualité est vraiment élevée.\n"\
"\n"\
"     \"preset=extreme\" Si vous avez une très bonne audition, ainsi que du\n"\
"                             matériel de qualité, ce pré-réglage offrira\n"\
"                             une qualité légèrement supérieure à celle du\n"\
"                             mode \"standard\"\n"\
"\n"\
"   Pour le CBR à 320kbps (la plus haute qualité possible avec les pré-réglages):\n"\
"\n"\
"     \"preset=insane\"  Ce réglage sera excessif pour la plupart des gens\n"\
"                             et des situations mais, si vous devez absolument\n"\
"                             avoir la plus haute qualité et que vous n'avez pas\n"\
"                             de contrainte de taille, choisissez cette option.\n"\
"\n"\
"   Pour les modes ABR (haute qualité pour un bitrate donnée - mais moins que pour du VBR) :\n"\
"\n"\
"     \"preset=<kbps>\"  Utiliser ce pré-réglage fournira une bonne qualité\n"\
"                             pour un bitrate spécifié. Selon le bitrate\n"\
"                             entré, ce pré-réglage déterminera les réglages\n"\
"                             optimaux pour cette situation particulière.\n"\
"                             Bien que cette approche fonctionne, elle n'est pas\n"\
"                             aussi flexible que le VBR, et n'offrira pas en général\n"\
"                             les mêmes niveaux que ceux du VBR aux bitrates élevés.\n"\
"\n"\
"Les options suivantes sont aussi disponibles pour les profils correspondants :\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Mode ABR) - C'est le mode par défaut. Pour l'utiliser,\n"\
"                      il suffit de préciser un bitrate. Par exemple :\n"\
"                      \"preset=185\" active ce pré-réglage\n"\
"                      et utilise un bitrate moyen de 185kbps.\n"\
"\n"\
"   \"fast\" - Active le nouveau mode rapide VBR pour un profil donné. Les\n"\
"              désavantages de cette option sont que, souvent, le bitrate\n"\
"              final sera légèrement plus élevé que pour le mode normal\n"\
"              et que la qualité peut aussi être légèrement inférieure.\n"\
"  Attention : avec la version actuelle, les pré-réglages en mode 'fast'\n"\
"              peuvent utiliser des bitrates trop élevés par rapport à ceux\n"\
"              des pré-réglages normaux.\n"\
"\n"\
"   \"cbr\"  - Si vous utilisez le mode ABR (voir ci-dessus) avec un bitrate\n"\
"              spécial tel que 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"              vous pouvez utiliser l'option \"cbr\" pour forcer une compression\n"\
"              en CBR au lieu d'ABR. ABR fournit une qualité plus élevée\n"\
"              mais le CBR peut être utile dans certains cas comme par exemple, pour\n"\
"              pour distribuer un flux MP3 sur Internet.\n"\
"\n"\
"    Par exemple :\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" ou \"-lameopts  cbr:preset=192       \"\n"\
" ou \"-lameopts      preset=172       \"\n"\
" ou \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Quelques noms de pré-réglages sont disponibles pour le mode ABR :\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit "Ne peux pas régler les options de LAME, vérifiez dans bitrate/samplerate,"\
"certains bitrates très bas (<32) requièrent des taux d'échantillonages plus bas (i.e. -srate 8000)."\
"Si rien ne marche, essayez un pré-réglage (preset)."
#define MSGTR_ConfigFileError "Erreur du fichier de configuration"
#define MSGTR_ErrorParsingCommandLine "Erreur en analysant la ligne de commande"
#define MSGTR_VideoStreamRequired "La présence d'un flux vidéo est obligatoire !\n"
#define MSGTR_ForcingInputFPS "Le fps d'entrée sera plutôt interprété comme %5.3f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Le format de sortie RAWVIDEO ne supporte pas l'audio - audio désactivé\n"
#define MSGTR_DemuxerDoesntSupportNosound "Ce demuxer ne supporte pas encore l'option -nosound.\n"
#define MSGTR_MemAllocFailed "Une allocation mémoire a échoué\n"
#define MSGTR_NoMatchingFilter "N'a pas pu trouver une correspondance filtre/ao !\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, peut-être un compilateur C cassé ?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, nom de codec manquant !\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, encodeur pour le codec %s introuvable !\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, échec lors de l'allocation du contexte !\n"
#define MSGTR_CouldntOpenCodec "Échec de l'ouverture du codec %s, br=%d\n"
#define MSGTR_CantCopyAudioFormat "Le format audio 0x%x est incompatible avec '-oac copy', veuillez essayer '-oac pcm' à la place, ou bien utilisez '-fafmttag' pour forcer ce mode.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     méthode à débit binaire (bitrate) variable\n"\
"                0 : cbr (débit binaire constant)\n"\
"                1 : mt (Mark Taylor)\n"\
"                2 : rh (Robert Hegemann) (par défaut)\n"\
"                3 : abr (débit binaire disponible)\n"\
"                4 : mtrh (Mark Taylor Robert Hegemann)\n"\
"\n"\
" abr           débit binaire (bitrate) disponible\n"\
"\n"\
" cbr           débit binaire (bitrate) constant\n"\
"               Force également l'encodage en mode CBR sur les modes préréglés ABR subsequents\n"\
"\n"\
" br=<0-1024>   spécifie le débit binaire (bitrate) en kbits (CBR et ABR uniquement)\n"\
"\n"\
" q=<0-9>       qualité (0-plus haute, 9-plus basse) (uniquement pour VBR)\n"\
"\n"\
" aq=<0-9>      qualité algorithmique (0-meilleure/plus lente, 9-pire/plus rapide)\n"\
"\n"\
" ratio=<1-100> rapport de compression\n"\
"\n"\
" vol=<0-10>    définit le gain d'entrée audio\n"\
"\n"\
" mode=<0-3>    (par défaut : auto)\n"\
"                0 : stereo\n"\
"                1 : stéréo commune (joint-stereo)\n"\
"                2 : canal double (dualchannel)\n"\
"                3 : mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0 : non\n"\
"                1 : tout\n"\
"                2 : ajuste\n"\
"\n"\
" fast          accélère l'encodage pour les modes préréglés VBR subséquents,\n"\
"               qualité légèrement inférieure et débits binaires (bitrates) plus élevés.\n"\
"\n"\
" preset=<valeur> fournit les plus hauts paramètres de qualité possibles.\n"\
"                 medium : encodage VBR, bonne qualité\n"\
"                 (intervalle de débit binaire (bitrate) 150-180 kbps)\n"\
"                 standard : encodage VBR, haute qualité\n"\
"                 (intervalle de débit binaire (bitrate) 170-210 kbps)\n"\
"                 extreme : encodage VBR, très haute qualité\n"\
"                 (intervalle de débit binaire (bitrate) 200-240 kbps)\n"\
"                 insane : encodage CBR, plus haute qualité préréglée\n"\
"                 (bitrate 320 kbps)\n"\
"                 <8-320> : encodage ABR au débit binaire (bitrate) moyen indiqué en kbps.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "code FourCC dupliqué"
#define MSGTR_TooManyFourccs "trop de FourCCs..."
#define MSGTR_ParseError "erreur de syntaxe"
#define MSGTR_ParseErrorFIDNotNumber "erreur de syntaxe (l'ID du format n'est pas un nombre ?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "erreur de syntaxe (l'ID de l'alias n'est pas un nombre ?)"
#define MSGTR_DuplicateFID "ID du format dupliqué"
#define MSGTR_TooManyOut "Trop de 'out'..."
#define MSGTR_InvalidCodecName "\nLe nom de codec (%s) n'est pas valide !\n"
#define MSGTR_CodecLacksFourcc "\nLe nom de codec(%s) n'a pas de FourCC !\n"
#define MSGTR_CodecLacksDriver "\nLe codec (%s) n'a pas de pilote !\n"
#define MSGTR_CodecNeedsDLL "\nLe codec (%s) requiert une 'dll' !\n"
#define MSGTR_CodecNeedsOutfmt "\nLe codec (%s) requiert un 'outfmt' !\n"
#define MSGTR_CantAllocateComment "Ne peux allouer de mémoire pour le commentaire. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN !"
#define MSGTR_ReadingFile "Lecture de %s: "
#define MSGTR_CantOpenFileError "Ne peux ouvrir '%s' : %s\n"
#define MSGTR_CantGetMemoryForLine "Ne peux allouer de mémoire pour 'line' : %s\n"
#define MSGTR_CantReallocCodecsp "Ne peux pas effectuer de realloc() pour '*codecsp' : %s\n"
#define MSGTR_CodecNameNotUnique "Le nom du codec '%s' n'est pas unique."
#define MSGTR_CantStrdupName "Ne peux appeler strdup() -> 'name' : %s\n"
#define MSGTR_CantStrdupInfo "Ne peux appler strdup() -> 'info' : %s\n"
#define MSGTR_CantStrdupDriver "Ne peux appeler strdup() -> 'driver' : %s\n"
#define MSGTR_CantStrdupDLL "Ne peux appeler strdup() -> 'dll' : %s"
#define MSGTR_AudioVideoCodecTotals "%d codecs audio & %d codecs vidéo\n"
#define MSGTR_CodecDefinitionIncorrect "Le codec n'est pas défini correctement."
#define MSGTR_OutdatedCodecsConf "Ce fichier codecs.conf est trop vieux et est incompatible avec cette version de MPlayer !"

// fifo.c
#define MSGTR_CannotMakePipe "Ne peux créer de canal de communication (pipe) !\n"

// parser-mecmd.c, parser-mpcmd.c
#define MSGTR_NoFileGivenOnCommandLine "'--' indique la fin des options, mais aucun nom de fichier fourni dans la commande.\n"
#define MSGTR_TheLoopOptionMustBeAnInteger "L'option loop doit être un entier : %s\n"
#define MSGTR_UnknownOptionOnCommandLine "Option non reconnue dans la ligne de commande : -%s\n"
#define MSGTR_ErrorParsingOptionOnCommandLine "Erreur lors de l'analyse des options de la ligne de commande : -%s\n"

#define MSGTR_NotAnMEncoderOption "-%s n'est pas une option de MEncoder\n"
#define MSGTR_NoFileGiven "Pas de fichier fourni\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Case de sauvegarde trouvée est trop ancienne  lvl %d : %d !!!\n"
#define MSGTR_InvalidCfgfileOption "L'option '%s' ne peut être utilisée dans un fichier de configuration.\n"
#define MSGTR_InvalidCmdlineOption "L'option '%s' ne peut être utilisée sur la ligne de commande.\n"
#define MSGTR_InvalidSuboption "Erreur : l'option '%s' n'a pas de sous-option '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Erreur : la sous-option '%s' de '%s' doit avoir un paramètre !\n"
#define MSGTR_MissingOptionParameter "Erreur : l'option '%s' doit avoir un paramètre !\n"
#define MSGTR_OptionListHeader "\n Nom                  Type            Min        Max      Global  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nTotal : %d options\n"
#define MSGTR_NoProfileDefined "Aucun profil n'a été défini.\n"
#define MSGTR_AvailableProfiles "Profils disponibles :\n"
#define MSGTR_UnknownProfile "Profil inconnu '%s'.\n"
#define MSGTR_Profile "Profil %s : %s\n"
// m_property.c
#define MSGTR_PropertyListHeader "\n Nom                  Type            Min        Max\n\n"
#define MSGTR_TotalProperties "\nTotal : %d propriétés\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Lecteur CD-ROM '%s' non trouvé.\n"
#define MSGTR_ErrTrackSelect "Erreur lors du choix de la piste VCD.\n"
#define MSGTR_ReadSTDIN "Lecture depuis stdin...\n"
#define MSGTR_UnableOpenURL "Impossible d'ouvrir l'URL : %s\n"
#define MSGTR_ConnToServer "Connecté au serveur : %s\n"
#define MSGTR_FileNotFound "Fichier non trouvé : '%s'\n"

#define MSGTR_SMBInitError "Impossible d'initialiser libsmbclient : %d\n"
#define MSGTR_SMBFileNotFound "Impossible d'ouvrir depuis le réseau local : '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer n'a pas été compilé avec le support de lecture SMB\n"

#define MSGTR_CantOpenDVD "Impossible d'ouvrir le lecteur DVD : %s (%s)\n"

// stream_dvd.c
#define MSGTR_DVDspeedCantOpen "Impossible d'ouvrir le lecteur DVD en écriture.  Changer la vitesse du DVD requière un accès en écriture.\n"
#define MSGTR_DVDrestoreSpeed "Remise en l'état de la vitesse du DVD... "
#define MSGTR_DVDlimitSpeed "Limite la vitesse du DVD à %dKo/s... "
#define MSGTR_DVDlimitFail "échoue\n"
#define MSGTR_DVDlimitOk "réussi\n"
#define MSGTR_NoDVDSupport "MPlayer a été compilé sans support pour les DVD - terminaison\n"
#define MSGTR_DVDnumTitles "Il y a %d titres sur ce DVD.\n"
#define MSGTR_DVDinvalidTitle "Numéro de titre DVD invalide : %d\n"
#define MSGTR_DVDnumChapters "Il y a %d chapitres sur ce titre DVD.\n"
#define MSGTR_DVDinvalidChapter "Numéro de chapitre DVD invalide : %d\n"
#define MSGTR_DVDinvalidChapterRange "Intervalle des chapitre invalide %s\n"
#define MSGTR_DVDinvalidLastChapter "Numéro de dernier chapitre du DVD invalide : %d\n"
#define MSGTR_DVDnumAngles "Il y a %d angles sur ce titre DVD.\n"
#define MSGTR_DVDinvalidAngle "Numéro d'angle DVD invalide : %d\n"
#define MSGTR_DVDnoIFO "Impossible d'ouvrir le fichier IFO pour le titre DVD %d.\n"
#define MSGTR_DVDnoVMG "Ne peut ouvrir les informations VMG !\n"
#define MSGTR_DVDnoVOBs "Impossible d'ouvrir le titre VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "Aucun canal audio correspondant sur ce DVD !\n"
#define MSGTR_DVDaudioChannel "Canal audio du DVD choisi: %d langue : %c%c\n"
#define MSGTR_DVDnoMatchingSubtitle "Aucun sous-titre correspondant sur ce DVD !\n"
#define MSGTR_DVDsubtitleChannel "Canal de sous-titres du DVD choisi : %d langue : %c%c\n"
#define MSGTR_DVDaudioStreamInfo "Flux audio : %d format : %s (%s) langue : %s aide : %d.\n"
#define MSGTR_DVDnumAudioChannels "Nombre du canaux audio sur le disque : %d.\n"
#define MSGTR_DVDsubtitleLanguage "Sous-titre ( sid ) : %d langue : %s\n"
#define MSGTR_DVDnumSubtitles "Nombre de sous-titres sur le disque : %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Trop de flux !"
#define MSGTR_RawMuxerOnlyOneStream "Le multiplexeur RAWAUDIO ne supporte qu'un seul flux audio!\n"
#define MSGTR_IgnoringVideoStream "Flux vidéo non pris en compte !\n"
#define MSGTR_UnknownStreamType "Attention ! flux de type inconnu : %d\n"
#define MSGTR_WarningLenIsntDivisible "Attention ! la longueur 'len' n'est pas divisible par la taille de l'échantillon (!\n"
#define MSGTR_MuxbufMallocErr "Tampon d'image Muxeur ne peut allouer de la mémoire !\n"
#define MSGTR_MuxbufReallocErr "Tampon d'image Muxeur ne peut réallouer de la mémoire !\n"
#define MSGTR_MuxbufSending "Tampon d'image Muxeur envoie %d image(s) au muxeur.\n"
#define MSGTR_WritingHeader "Écriture de l'entête...\n"
#define MSGTR_WritingTrailer "Écriture de l'index...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "ATTENTION : Entête du flux audio %d redéfini.\n"
#define MSGTR_VideoStreamRedefined "ATTENTION : Entête du flux vidéo %d redéfini.\n"
#define MSGTR_TooManyAudioInBuffer "\nTrop de paquets audio dans le tampon (%d dans %d octets)\n"
#define MSGTR_TooManyVideoInBuffer "\nTrop de paquets vidéo dans le tampon (%d dans %d octets)\n"
#define MSGTR_MaybeNI "Peut-être que vous jouez un flux/fichier non entrelacé, ou que le codec a échoué ?\n"\
                      "Pour les fichier AVI, essayez de forcer le mode non-entrelacé avec l'option -ni.\n"
#define MSGTR_WorkAroundBlockAlignHeaderBug "AVI : Bogue entête de la solution de contournement CBR-MP3 nBlockAlign !\n"
#define MSGTR_SwitchToNi "\nFichier AVI mal entrelacé - passage en mode -ni...\n"
#define MSGTR_InvalidAudioStreamNosound "AVI : ID de flux audio invalide : %d - ignorer (pas de son)\n"
#define MSGTR_InvalidAudioStreamUsingDefault "AVI : ID de flux vidéo invalide : %d - ignorer (utilise défaut)\n"
#define MSGTR_ON2AviFormat "Format ON2 AVI"
#define MSGTR_Detected_XXX_FileFormat "Fichier de type %s détecté.\n"
#define MSGTR_DetectedAudiofile "Fichier audio détecté.\n"
#define MSGTR_NotSystemStream "Pas un flux de type MPEG System... (peut-être un Flux de Transport ?)\n"
#define MSGTR_InvalidMPEGES "Flux MPEG-ES invalide ??? Contactez l'auteur, c'est peut-être un bogue :(\n"
#define MSGTR_FormatNotRecognized "========== Désolé, ce format de fichier n'est pas reconnu/supporté ============\n"\
                                  "== Si ce fichier est un flux AVI, ASF ou MPEG, merci de contacter l'auteur ! ==\n"
#define MSGTR_SettingProcessPriority "Réglage de la priorité du process: %s\n"
#define MSGTR_FilefmtFourccSizeFpsFtime "[V] filefmt:%d  fourcc:0x%X  taille:%dx%d  fps:%5.3f  ftime:=%6.4f\n"
#define MSGTR_CannotInitializeMuxer "Impossible d'initialiser le muxeur."
#define MSGTR_MissingVideoStream "Aucun flux vidéo trouvé.\n"
#define MSGTR_MissingAudioStream "Aucun flux audio trouvé -> pas de son\n"
#define MSGTR_MissingVideoStreamBug "Flux vidéo manquant !? Contactez l'auteur, c'est peut-être un bogue :(\n"

#define MSGTR_DoesntContainSelectedStream "demux : le fichier ne contient pas le flux audio ou vidéo sélectionné.\n"

#define MSGTR_NI_Forced "Forcé"
#define MSGTR_NI_Detected "Détecté"
#define MSGTR_NI_Message "format de fichier AVI NON-ENTRELACÉ %s.\n"

#define MSGTR_UsingNINI "Utilise le format des fichiers AVI endommagés NON-ENTRELACÉ.\n"
#define MSGTR_CouldntDetFNo "Impossible de déterminer le nombre d'images (pour recherche absolue)\n"
#define MSGTR_CantSeekRawAVI "Impossible de chercher dans un flux AVI brut ! (Index requis, essayez l'option -idx.)\n"
#define MSGTR_CantSeekFile "Impossible de chercher dans ce fichier.\n"

#define MSGTR_MOVcomprhdr "MOV : Le support d'entêtes compressées nécessite ZLIB !\n"
#define MSGTR_MOVvariableFourCC "MOV : ATTENTION : FOURCC Variable détecté !?\n"
#define MSGTR_MOVtooManyTrk "MOV : ATTENTION : Trop de pistes"
#define MSGTR_FoundAudioStream "==> Flux audio trouvé : %d\n"
#define MSGTR_FoundVideoStream "==> Flux vidéo trouvé : %d\n"
#define MSGTR_DetectedTV "TV détectée ! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Impossible d'ouvrir le demuxer Ogg\n"
#define MSGTR_ASFSearchingForAudioStream "ASF : recherche du flux audio (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Impossible d'ouvrir le flux audio : %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Impossible d'ouvrir le flux des sous-titres : %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Echec à l'ouverture du demuxer audio : %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Echec à l'ouverture du demuxer de sous-titres : %s\n"
#define MSGTR_TVInputNotSeekable "Impossible de rechercher sur l'entrée TV ! (cette opération correspondra sûrement à un changement de chaines ;)\n"
#define MSGTR_DemuxerInfoChanged "Info demuxer %s changé à %s\n"
#define MSGTR_ClipInfo "Information sur le clip : \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg : contenu NTSC 30000/1001fps détecté, ajustement du débit.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg : contenu NTSC 24000/1001fps progressif détecté, ajustement du débit.\n"
#define MSGTR_CacheFill "\rRemplissage du cache : %5.2f%% (%"PRId64" octets)   "
#define MSGTR_NoBindFound "Aucune action attachée à la touche '%s'"
#define MSGTR_FailedToOpen "Échec à l'ouverture de '%s'\n"

#define MSGTR_VideoID "[%s] Flux vidéo trouvé, -vid %d\n"
#define MSGTR_AudioID "[%s] Flux audio trouvé, -aid %d\n"
#define MSGTR_SubtitleID "[%s] Subtitle stream found, -sid %d\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Impossible d'ouvrir le codec.\n"
#define MSGTR_CantCloseCodec "Impossible de fermer le codec.\n"

#define MSGTR_MissingDLLcodec "ERREUR : Impossible d'ouvrir le codec DirectShow requis : %s\n"
#define MSGTR_ACMiniterror "Impossible de charger/initialiser le codec AUDIO Win32/ACM (fichier DLL manquant ?)\n"
#define MSGTR_MissingLAVCcodec "Impossible de trouver le codec '%s' dans libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG : FATAL : Fin du fichier lors de la recherche d'entête de séquence\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL : Ne peut lire l'entête de séquence.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL : Ne peut lire l'extension d'entête de séquence.\n"
#define MSGTR_BadMpegSequHdr "MPEG : Mauvaise entête de séquence\n"
#define MSGTR_BadMpegSequHdrEx "MPEG : Mauvaise extension d'entête de séquence\n"

#define MSGTR_ShMemAllocFail "Impossible d'allouer la mémoire partagée\n"
#define MSGTR_CantAllocAudioBuf "Impossible d'allouer le tampon de sortie audio\n"

#define MSGTR_UnknownAudio "Format audio inconnu/manquant -> pas de son\n"

#define MSGTR_UsingExternalPP "[PP] Utilisation de filtres de postprocessing externes, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Utilisation du postprocessing du codec, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "L'attribut vidéo '%s' n'est pas supporté par ce vo & ce vd. \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Famille de codecs vidéo demandée [%s] (vfm=%s) non disponible (activez-la à la compilation)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Famille de codecs audio demandée [%s] (afm=%s) non disponible (activez-la à la compilation)\n"
#define MSGTR_OpeningVideoDecoder "Ouverture du décodeur vidéo : [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Codec vidéo choisi : [%s] vfm : %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Ouverture décodeur audio : [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Codec audio sélectionné : [%s] afm : %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Création chaine filtre audio pour %dHz/%dch/%s -> %dHz/%dch/%s...\n"
#define MSGTR_UninitVideoStr "Désinitialisation vidéo : %s  \n"
#define MSGTR_UninitAudioStr "Désinitialisation audio : %s  \n"
#define MSGTR_VDecoderInitFailed "Echec de l'initialisation de VDecoder :(\n"
#define MSGTR_ADecoderInitFailed "Echec de l'initialisation de ADecoder :(\n"
#define MSGTR_ADecoderPreinitFailed "Echec de la pré-initialisation de l'ADecoder :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: allocation de %d octets comme tampon d'entrée\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio : allocation %d + %d = %d octets comme tampon de sortie\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Mise en place du support LIRC...\n"
#define MSGTR_LIRCopenfailed "Impossible d'activer le support LIRC.\n"
#define MSGTR_LIRCcfgerr "Impossible de lire le fichier de config de LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Impossible de trouver le filtre vidéo '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Impossible d'ouvrir le filtre vidéo '%s'\n"
#define MSGTR_OpeningVideoFilter "Ouverture du filtre vidéo : "
#define MSGTR_CannotFindColorspace "Impossible de trouver espace colorimétrique assorti, même en utilisant 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec : le codec n'a pas défini sh->disp_w et sh->disp_h, essai de contournement !\n"
#define MSGTR_VoConfigRequest "VDec : requête de config de vo - %d x %d (espace colorimétrique préferé : %s)\n"
#define MSGTR_CouldNotFindColorspace "N'a pas pu trouver espace colorimétrique correspondant - nouvel essai avec -vf scale...\n"
#define MSGTR_MovieAspectIsSet "L'aspect du film est %.2f:1 - pré-redimensionnement à l'aspect correct.\n"
#define MSGTR_MovieAspectUndefined "L'aspect du film est indéfini - pas de pré-dimensionnement appliqué.\n"
// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Vous devez mettre à jour/installer le package contenant les codecs binaires.\nAllez sur http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO : initialisation réussie du codec vidéo Win32/DShow.\n"
#define MSGTR_DMOInitOK "INFO : initialisation réussie du codec vidéo Win32/DMO.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11 : n'a pas pu envoyer l'événement EWMH pour passer en plein écran !\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable : n'a pas pu trouver de fenêtre XScreenSaver.\n"
#define MSGTR_SelectedVideoMode "XF86VM : le mode vidéo %dx%d a été choisi pour une taille d'image %dx%d.\n"

#define MSGTR_InsertingAfVolume "[Mixer] Pas de support matériel pour le mixage, insertion du filtre logiciel de volume.\n"
#define MSGTR_NoVolume "[Mixer] Aucun contrôle de volume disponible.\n"
#define MSGTR_NoBalance "[Mixer] Aucun contrôle de balance disponible.\n"

// ====================== messages/boutons GUI ========================

#ifdef CONFIG_GUI

// --- labels ---
#define MSGTR_About "À propos..."
#define MSGTR_FileSelect "Choisir un fichier..."
#define MSGTR_SubtitleSelect "Choisir un sous-titre..."
#define MSGTR_OtherSelect "Choisir..."
#define MSGTR_AudioFileSelect "Choisir une source audio extérieure..."
#define MSGTR_FontSelect "Choisir une police..."
#define MSGTR_PlayList "Liste de lecture"
#define MSGTR_Equalizer "Égalisateur"
#define MSGTR_ConfigureEqualizer "Configure Égalisateur"
#define MSGTR_SkinBrowser "Navigateur de peaux"
#define MSGTR_Network "Streaming depuis le réseau ..."
#define MSGTR_Preferences "Préférences"
#define MSGTR_AudioPreferences "Configuration de pilote Audio"
#define MSGTR_NoMediaOpened "Aucun média ouvert"
#define MSGTR_VCDTrack "Piste du VCD %d"
#define MSGTR_NoChapter "Aucun chapitre"
#define MSGTR_Chapter "Chapitre %d"
#define MSGTR_NoFileLoaded "Aucun fichier chargé"

// --- boutons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Annuler"
#define MSGTR_Add "Ajouter"
#define MSGTR_Remove "Supprimer"
#define MSGTR_Clear "Effacer"
#define MSGTR_Config "Configurer"
#define MSGTR_ConfigDriver "Configuration du pilote"
#define MSGTR_Browse "Naviguer"

// --- messages d'erreur ---
#define MSGTR_NEMDB "Désolé, pas assez de mémoire pour le tampon de dessin."
#define MSGTR_NEMFMR "Désolé, pas assez de mémoire pour le rendu des menus."
#define MSGTR_IDFGCVD "Désolé, aucun pilote de sortie vidéo compatible avec la GUI."
#define MSGTR_NEEDLAVC "Désolé, vous ne pouvez pas lire de fichiers non-MPEG avec le périphérique DXR3/H+ sans réencoder.\nActivez plutôt lavc dans la boîte de configuration DXR3/H+."
#define MSGTR_UNKNOWNWINDOWTYPE "Genre de fenêtre inconnue trouvé ..."

// --- messages d'erreurs du chargement de peau ---
#define MSGTR_SKIN_ERRORMESSAGE "[Peau] erreur à la ligne %d du fichier de config de peau : %s"
#define MSGTR_SKIN_WARNING1 "[Peau] attention à la ligne %d du fichier de config de peau : Widget (%s) trouvé mais aucune \"section\" trouvé avant lui."
#define MSGTR_SKIN_WARNING2 "[Peau] attention à la ligne %d du fichier de config de peau : Widget (%s) trouvé mais aucune \"subsection\" trouvé avant lui."
#define MSGTR_SKIN_WARNING3 "[Peau] attention à la ligne %d du fichier de config de peau : cette sous-section n'est pas supporté par le widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[peau] fichier ( %s ) non trouvé.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[peau] fichier ( %s ) non lisible.\n"
#define MSGTR_SKIN_BITMAP_16bit  "les images bitmaps 16 bits ou moins ne sont pas supportées ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Fichier non trouvé (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "erreur de lecture BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "erreur de lecture TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "erreur de lecture PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "tga compacté en RLE non supporté (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "format de fichier inconnu (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "Erreur de conversion 24 bit vers 32 bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "message inconnu : %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "pas assez de mémoire\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "trop de polices déclarées.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "fichier de police introuvable.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "fichier d'image de police introuvable\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificateur de fonte inéxistant (%s)\n"
#define MSGTR_SKIN_UnknownParameter "paramètre inconnu (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin non trouvée (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Peau choisi ( %s ) non trouvé, essaie de 'par défaut'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "erreur de lecture du fichier de configuration du peau (%s)\n"
#define MSGTR_SKIN_LABEL "Peaux :"

// --- menus gtk
#define MSGTR_MENU_AboutMPlayer "À propos de MPlayer"
#define MSGTR_MENU_Open "Ouvrir..."
#define MSGTR_MENU_PlayFile "Lire un fichier..."
#define MSGTR_MENU_PlayVCD "Lire un VCD..."
#define MSGTR_MENU_PlayDVD "Lire un DVD..."
#define MSGTR_MENU_PlayURL "Lire une URL..."
#define MSGTR_MENU_LoadSubtitle "Charger un sous-titre..."
#define MSGTR_MENU_DropSubtitle "Laisser tomber un sous-titre..."
#define MSGTR_MENU_LoadExternAudioFile "Chargement d'un fichier audio externe..."
#define MSGTR_MENU_Playing "En cours de lecture"
#define MSGTR_MENU_Play "Lecture"
#define MSGTR_MENU_Pause "Pause"
#define MSGTR_MENU_Stop "Arrêt"
#define MSGTR_MENU_NextStream "Flux suivant"
#define MSGTR_MENU_PrevStream "Flux précédent"
#define MSGTR_MENU_Size "Taille"
#define MSGTR_MENU_HalfSize   "Demi taille"
#define MSGTR_MENU_NormalSize "Taille normale"
#define MSGTR_MENU_DoubleSize "Taille double"
#define MSGTR_MENU_FullScreen "Plein écran"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Ouvrir un disque..."
#define MSGTR_MENU_ShowDVDMenu "Afficher le menu DVD"
#define MSGTR_MENU_Titles "Titres"
#define MSGTR_MENU_Title "Titre %2d"
#define MSGTR_MENU_None "(aucun)"
#define MSGTR_MENU_Chapters "Chapitres"
#define MSGTR_MENU_Chapter "Chapitre %2d"
#define MSGTR_MENU_AudioLanguages "Langues audio"
#define MSGTR_MENU_SubtitleLanguages "Langues des sous-titres"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Navigateur de peaux"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Quitter..."
#define MSGTR_MENU_Mute "Silence"
#define MSGTR_MENU_Original "Original"
#define MSGTR_MENU_AspectRatio "rapport hauteur/largeur"
#define MSGTR_MENU_AudioTrack "Piste audio"
#define MSGTR_MENU_Track "Piste %d"
#define MSGTR_MENU_VideoTrack "Piste Vidéo"
#define MSGTR_MENU_Subtitles "Sous-titres"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Vidéo"
#define MSGTR_EQU_Contrast "Contraste : "
#define MSGTR_EQU_Brightness "Luminosité : "
#define MSGTR_EQU_Hue "Tonalité : "
#define MSGTR_EQU_Saturation "Saturation : "
#define MSGTR_EQU_Front_Left "Avant Gauche"
#define MSGTR_EQU_Front_Right "Avant Droit"
#define MSGTR_EQU_Back_Left "Arrière Gauche"
#define MSGTR_EQU_Back_Right "Arrière Droit"
#define MSGTR_EQU_Center "Centre"
#define MSGTR_EQU_Bass "Basses"
#define MSGTR_EQU_All "Tout"
#define MSGTR_EQU_Channel1 "Canal 1 :"
#define MSGTR_EQU_Channel2 "Canal 2 :"
#define MSGTR_EQU_Channel3 "Canal 3 :"
#define MSGTR_EQU_Channel4 "Canal 4 :"
#define MSGTR_EQU_Channel5 "Canal 5 :"
#define MSGTR_EQU_Channel6 "Canal 6 :"

// --- playlist
#define MSGTR_PLAYLIST_Path "Chemin"
#define MSGTR_PLAYLIST_Selected "Fichiers choisis"
#define MSGTR_PLAYLIST_Files "Fichiers"
#define MSGTR_PLAYLIST_DirectoryTree "Hiérarchie des dossiers"

// --- preferences
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "Sous-titres & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs & demuxeur"
#define MSGTR_PREFERENCES_Misc "Divers"

#define MSGTR_PREFERENCES_None "Aucun"
#define MSGTR_PREFERENCES_DriverDefault "Pilote par défaut"
#define MSGTR_PREFERENCES_AvailableDrivers "Pilotes disponibles :"
#define MSGTR_PREFERENCES_DoNotPlaySound "Ne pas jouer le son"
#define MSGTR_PREFERENCES_NormalizeSound "Normaliser le son"
#define MSGTR_PREFERENCES_EnableEqualizer "Egaliseur (Equalizer) activé"
#define MSGTR_PREFERENCES_SoftwareMixer "Activer mixeur logiciel"
#define MSGTR_PREFERENCES_ExtraStereo "Activer stéréo supplémentaire"
#define MSGTR_PREFERENCES_Coefficient "Coefficient :"
#define MSGTR_PREFERENCES_AudioDelay "Retard audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Activer tampon double"
#define MSGTR_PREFERENCES_DirectRender "Activer le rendu direct"
#define MSGTR_PREFERENCES_FrameDrop "Activer les sauts d'images"
#define MSGTR_PREFERENCES_HFrameDrop "Activer saut DUR d'images (dangereux)"
#define MSGTR_PREFERENCES_Flip "Mirroir vertical"
#define MSGTR_PREFERENCES_Panscan "Recadrage : "
#define MSGTR_PREFERENCES_OSDTimer "Minuteur et indicateurs"
#define MSGTR_PREFERENCES_OSDProgress "Barres de progression seulement"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Minuteur, pourcentage et temps total"
#define MSGTR_PREFERENCES_Subtitle "Sous-titre :"
#define MSGTR_PREFERENCES_SUB_Delay "Décalage : "
#define MSGTR_PREFERENCES_SUB_FPS "FPS :"
#define MSGTR_PREFERENCES_SUB_POS "Position : "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Désactiver le chargement automatique des sous-titres"
#define MSGTR_PREFERENCES_SUB_Unicode "Sous-titre en Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Convertir le sous-titre au format MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Convertir le sous-titre au format SubViewer (SRT) basé sur le temps"
#define MSGTR_PREFERENCES_SUB_Overlap "Bascule le recouvrement des sous-titres"
#define MSGTR_PREFERENCES_SUB_USE_ASS "Restitution sous-titre SSA/ASS"
#define MSGTR_PREFERENCES_SUB_ASS_USE_MARGINS "Utilise les marges"
#define MSGTR_PREFERENCES_SUB_ASS_TOP_MARGIN "Haut : "
#define MSGTR_PREFERENCES_SUB_ASS_BOTTOM_MARGIN "Bas : "
#define MSGTR_PREFERENCES_Font "Police :"
#define MSGTR_PREFERENCES_FontFactor "Facteur de police :"
#define MSGTR_PREFERENCES_PostProcess "Activer le postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Qualité auto. : "
#define MSGTR_PREFERENCES_NI "Utiliser le parseur d'AVI non entrelacé"
#define MSGTR_PREFERENCES_IDX "Reconstruire l'index, si nécessaire"
#define MSGTR_PREFERENCES_VideoCodecFamily "Famille de codecs vidéo :"
#define MSGTR_PREFERENCES_AudioCodecFamily "Famille de codecs audio :"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Niveau OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Sous-titre"
#define MSGTR_PREFERENCES_FRAME_Font "Police"
#define MSGTR_PREFERENCES_FRAME_PostProcess "post-traitement"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "Périférique :"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixeur :"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Canal de mixeur :"
#define MSGTR_PREFERENCES_Message "ATTENTION : certaines options requièrent un redémarrage de la lecture !"
#define MSGTR_PREFERENCES_DXR3_VENC "Encodeur vidéo :"
#define MSGTR_PREFERENCES_DXR3_LAVC "Utiliser LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Langues Européennes Occidentales (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Langues Européeenes Occidentales avec Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Langues Européeenes Slaves/Centrales (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galicien, Maltais, Turc (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Caractères Old Baltic (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrillique (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabe (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Grec Moderne (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turc (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Balte (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celte (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebreu (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Russe (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainien, Biélorusse (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Chinois Simplifié (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Chinois Traditionnel (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japonais (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Coréen (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thaïlandais (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrillique Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Slave/Europe Centrale Windows (CP1250)"
#define MSGTR_PREFERENCES_FontEncoding23 "Arabe Windows (CP1256)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Pas d'agrandissement auto"
#define MSGTR_PREFERENCES_FontPropWidth "Proportionnel à la largeur du film"
#define MSGTR_PREFERENCES_FontPropHeight "Proportionnel à la hauteur du film"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proportionnel à la diagonale du film"
#define MSGTR_PREFERENCES_FontEncoding "Encodage :"
#define MSGTR_PREFERENCES_FontBlur "Flou :"
#define MSGTR_PREFERENCES_FontOutLine "Contour :"
#define MSGTR_PREFERENCES_FontTextScale "Echelle du texte :"
#define MSGTR_PREFERENCES_FontOSDScale "Echelle de l'OSD :"
#define MSGTR_PREFERENCES_Cache "Cache activé/désactivé"
#define MSGTR_PREFERENCES_CacheSize "Taille du cache : "
#define MSGTR_PREFERENCES_LoadFullscreen "Démarrer en plein écran"
#define MSGTR_PREFERENCES_SaveWinPos "Enrégistrer position de la fenêtre"
#define MSGTR_PREFERENCES_XSCREENSAVER "Arrêter XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Active la barre de lecture"
#define MSGTR_PREFERENCES_AutoSync "AutoSynchro on/off"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosynchro : "
#define MSGTR_PREFERENCES_CDROMDevice "Périphérique CD-ROM :"
#define MSGTR_PREFERENCES_DVDDevice "Périphérique DVD :"
#define MSGTR_PREFERENCES_FPS "FPS du film :"
#define MSGTR_PREFERENCES_ShowVideoWindow "Affiche la fenêtre vidéo inactive"
#define MSGTR_PREFERENCES_ArtsBroken "Versions aRts plus récentes sont incompatibles "\
           "avec GTK 1.x et feront planter GMPlayer!"

#define MSGTR_ABOUT_UHU "Le développement de la GUI est commandité par UHU Linux\n"
#define MSGTR_ABOUT_Contributors "Contributeurs de code et de documentation\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Codecs et libraries tiers\n"
#define MSGTR_ABOUT_Translations "Traductions\n"
#define MSGTR_ABOUT_Skins "Peaux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Erreur fatale !"
#define MSGTR_MSGBOX_LABEL_Error "Erreur !"
#define MSGTR_MSGBOX_LABEL_Warning "Attention !"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] mémoire insuffisante pour image\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] mémoire insuffisante pour image\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] Erreur lecture fichier config ...\n"
#define MSGTR_UnableToSaveOption "[cfg] Impossible de sauvegarder l'option '%s'.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Suppression des sous-titres.\n"
#define MSGTR_LoadingSubtitles "[GUI] Chargement des soustitres : %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Ajout de filtre vidéo : %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Enlèvement de filtre video : %s\n"

// mw.c

#define MSGTR_NotAFile "Ceci ne semble pas être un fichier : %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] Impossible d'ouvrir l'affichage.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Affichage à distance, désactive XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Désolé, votre système ne supporte pas l'extension de mémoire partagée X.\n"
#define MSGTR_WS_NoXshape "[ws] Désolé, votre système ne supporte pas l'extension XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Désolé, la profondeur d'échantillonnage est trop basse.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Trop de fenêtres ouvertes.\n"
#define MSGTR_WS_ShmError "[ws] Erreur d'extension de mémoire partagée\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Désolé, mémoire insuffisante pour tampon de dessin.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS non disponible ?\n"
#define MSGTR_WS_DpmsNotEnabled "Imposssible d'activer DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Ceci ne semble pas être un fichier...\n"
#define MSGTR_WS_DDNothing "D&D : Rien de retourné !\n"

#endif

// ======================= VO Pilotes Sortie Video ========================

#define MSGTR_VOincompCodec "Le périphérique de sortie vidéo sélectionné est incompatible avec ce codec.\n"\
                "Essayer d'ajouter le filtre d'échelle, e.g. -vf spp,scale plutôt que -vf spp.\n"
#define MSGTR_VO_GenericError "Cette erreur s'est produite"
#define MSGTR_VO_UnableToAccess "Impossible d'accéder"
#define MSGTR_VO_ExistsButNoDirectory "Existe déjà, mais n'est pas un répertoire."
#define MSGTR_VO_DirExistsButNotWritable "Répertoire de sortie existe déjà, mais n'est pas en écriture."
#define MSGTR_VO_DirExistsAndIsWritable "Répertoire de sortie existe déjà et n'est pas en écriture."
#define MSGTR_VO_CantCreateDirectory "Impossible de créer répertoire de sortie."
#define MSGTR_VO_CantCreateFile "Impossible de créer fichier de sortie."
#define MSGTR_VO_DirectoryCreateSuccess "Répertoire de sortie créé avec succès."
#define MSGTR_VO_ParsingSuboptions "Analyse de sous-options."
#define MSGTR_VO_SuboptionsParsedOK "sous-options analysées OK."
#define MSGTR_VO_ValueOutOfRange "Valeur hors plage"
#define MSGTR_VO_NoValueSpecified "Aucune valeur spécifiée."
#define MSGTR_VO_UnknownSuboptions "Sous-option(s) inconnue(s)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nVoici les sous-options aalib vo_aa :\n"
#define MSGTR_VO_AA_AdditionalOptions "Options supplémentaires fournies par vo_aa :\n" \
"  help        imprime ce message d'aide\n" \
"  osdcolor    met couleur osd\n  subcolor    met couleur sous-titre\n" \
"        les paramètres de couleur sont:\n           0 : normal\n" \
"           1 : faible\n           2 : fort\n        3 : police forte\n" \
"           4 : inversé\n          5 : spécial\n\n\n"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "JPEG progressif activé."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "JPEG progressif désactivé."
#define MSGTR_VO_JPEG_BaselineJPEG "Ligne de base JPEG activée."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Ligne de base JPEG désactivée."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Mode ASCII activé."
#define MSGTR_VO_PNM_RawMode "Mode cru activé."
#define MSGTR_VO_PNM_PPMType "Écriture de fichiers PPM."
#define MSGTR_VO_PNM_PGMType "Écriture de fichiers PGM."
#define MSGTR_VO_PNM_PGMYUVType "Écriture de fichiers PGMYUV."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Mode entrelacé requiert hauteur d'image divisible par 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Impossible d'allouer tampon de ligne pour mode entrelacé."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Entré non RGB, impossible décomposer chrominance !"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Largeur d'image doit être divisible par 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Mémoire insuffisante pour allouer tampon d'image RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Impossible d'obtenir ident. de fichier ou mémoire pour écriture \"%s\" !"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Erreur d'écriture d'image vers sortie !"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Sous-périphérique inconnu : %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Mode sortie entrelacée utilisée, champ haut au début."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Mode sortie entrelacée utilisée, champ bas au début."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Mode image progressive (par defaut) utilisé."

// sub.c
#define MSGTR_VO_SUB_Seekbar "Recherche"
#define MSGTR_VO_SUB_Play "Lecture"
#define MSGTR_VO_SUB_Pause "Pause"
#define MSGTR_VO_SUB_Stop "Arret"
#define MSGTR_VO_SUB_Rewind "Rembobine"
#define MSGTR_VO_SUB_Forward "Avant"
#define MSGTR_VO_SUB_Clock "Horloge"
#define MSGTR_VO_SUB_Contrast "Contraste"
#define MSGTR_VO_SUB_Saturation "Saturation"
#define MSGTR_VO_SUB_Volume "Volume"
#define MSGTR_VO_SUB_Brightness "Luminosité"
#define MSGTR_VO_SUB_Hue "Tonalité"
#define MSGTR_VO_SUB_Balance "Balance"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "Dimensions d'image source trop élevées: %ux%u (maximum %ux%u)\n"

// Anciens pilotes VO qui ont été remplacés

#define MSGTR_VO_PGM_HasBeenReplaced "Pilote sortie vidéo pgm remplacé par -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Pilote sortie vidéo md5 remplacé par -vo md5sum.\n"

// ======================= AO Pilote Sortie Audio ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out : modules alsa9 et alsa1x enlevés, utiliser plutôt -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup : Impossible d'ouvrir mixeur %s : %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup : Mixeur de carte audio n'a pas canal '%s' utilise default.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Impossible ouvrir périphérique audio %s : %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup : Impossible identifier desc de fichier gelé : %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Impossible de régler périphérique audio %s à sortie %s, essaie %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup : N'a pu régler périphérique audio à %d canaux.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup : Pilote ne supporte pas SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Votre pilote audio ne supporte PAS select()  ***\n Recompiler MPlayer avec #undef HAVE_AUDIO_SELECT dans config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nErreur fatale : *** IMPOSSIBLE RÉOUVRIR / REPARTIR PÉRIPHERIQUE AUDIO *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] Format OSS inconnu/non-supporté : %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Connecté au serveur de son.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Impossible ouvrir flux.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Flux ouvert.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] Grandeur tampon : %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] N'a pu régler volume à %d.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz non-supporté, essayer rééchantillonnage.\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] Echec de esd_open_sound : %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] Latence : [serveur : %0.2fs, net : %0.2fs] (ajuste %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Echec d'ouverture de flux rappel ESD : %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Echec mixeur ensemble audio DVB : %s.\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz non supporté, essayer rééchantillonnage.\n"

// ao_null.c
// Celui-ci n'a même aucun mp_msg ou printf's?? [VERIFIE]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Fichier : %s (%s)\nPCM : Échantillonnage : %iHz Canaux : %s Format %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info : Accélérer déchargement avec -vc null -vo null\n[AO PCM] Info : Pour écrire fichers WAVE utiliser -ao pcm:waveheader (par defaut).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Échec ouverture %s en écriture !\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Échantillonnage : %iHz Canaux : %s Format %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] Pilote audio %s utilisé.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Format audio non supporté : 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Échec initialisation audio SDL : %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Impossible ouvrir audio : %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] Contrôle.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] Init : Échantillonnage : %iHz Canaux : %s Format %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] Lecture : périphérique invalide.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] Init : échec setparams: %s\nImpossible régler échantillonnage désiré.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] Init : AL_RATE non acceptée sur la ressource donnée.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] Init : échec getparams : %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] Init : échantillonnage maintenant %lf (taux désiré : %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] Init : %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] Init : Impossible d'ouvrir canal audio : %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] Desinit : ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] Repart : ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] pause_audio : ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] repart_audio : ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc : échec SETINFO.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc : échec écriture.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Impossible d'ouvrir périphérique audio %s, %s  -> aucun son.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup : votre carte ne supporte pas canal %d, %s, %d Hz échantillonnage.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Votre pilote audio ne supporte PAS select()  ***\nRecompiler MPlayer avec #undef HAVE_AUDIO_SELECT dans config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nÉrreur fatale : *** IMPOSSIBLE DE RÉOUVRIR/REPARTIR PÉRIPHÉRIQUE AUDIO (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init : format requis : %d Hz, %d canaux, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init : aucune carte son trouvée.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init : format invalide (%s) requis - sortie désactivée.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init : erreur ouverture lecture : %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init : erreur pcm info : %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init : %d carte(s) son trouvée(s), utilise : %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init : erreur info canal pcm : %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init : erreur parametrage : %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init : erreur ouverture canal : %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init : erreur préparation canal : %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit : erreur drain de lecture : %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit : erreur vidage de lecture : %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit : erreur fermeture pcm : %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset : erreur drain de lecture : %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset : erreur vidage de lecture : %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset : erreur préparation canal : %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause : erreur drain de lecture : %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause : erreur vidage de lecture : %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume : erreur préparation canal : %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play : sous-passement alsa, réinit flux.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play : erreur préparation lecture : %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play : erreur écriture après réinit : %s - abandon.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play : erreur de sortie : %s\n"

// ao_alsa.c
#define MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero "[AO_ALSA] Index du mixeur invalide. Défaut à 0.\n"
#define MSGTR_AO_ALSA_MixerOpenError "[AO_ALSA] Erreur ouverture mixeur : %s\n"
#define MSGTR_AO_ALSA_MixerAttachError "[AO_ALSA] Erreur attachement mixeur %s : %s\n"
#define MSGTR_AO_ALSA_MixerRegisterError "[AO_ALSA] Erreur enregistrement mixeur : %s\n"
#define MSGTR_AO_ALSA_MixerLoadError "[AO_ALSA] Erreur chargement mixeur : %s\n"
#define MSGTR_AO_ALSA_UnableToFindSimpleControl "[AO_ALSA] Impossible de trouver un contrôle simple '%s',%i.\n"
#define MSGTR_AO_ALSA_ErrorSettingLeftChannel "[AO_ALSA] Erreur réglage canal gauche, %s\n"
#define MSGTR_AO_ALSA_ErrorSettingRightChannel "[AO_ALSA] Erreur réglage canal droit, %s\n"
#define MSGTR_AO_ALSA_CommandlineHelp "\n[AO_ALSA] -ao aide ALSA en ligne de commande :\n"\
"[AO_ALSA] Exemple : mplayer -ao alsa:device=hw=0.3\n"\
"[AO_ALSA]   Fixe 1° carte 4° périphérique matériel.\n\n"\
"[AO_ALSA] Options :\n"\
"[AO_ALSA]   noblock\n"\
"[AO_ALSA]     Ouvre le périphérique en mode non-bloqué.\n"\
"[AO_ALSA]   device=<device-name>\n"\
"[AO_ALSA]     met le périphérique (change , vers . et : vers =)\n"
#define MSGTR_AO_ALSA_ChannelsNotSupported "[AO_ALSA] canaux %d non supportés.\n"
#define MSGTR_AO_ALSA_OpenInNonblockModeFailed "[AO_ALSA] Echec ouverture en mode non-bloqué, essaie ouverture en mode bloqué.\n"
#define MSGTR_AO_ALSA_PlaybackOpenError "[AO_ALSA] Erreur ouverture de lecture : %s\n"
#define MSGTR_AO_ALSA_ErrorSetBlockMode "[AL_ALSA] Erreur mise en mode bloqué %s.\n"
#define MSGTR_AO_ALSA_UnableToGetInitialParameters "[AO_ALSA] Obtention impossible des paramètres initiaux : %s\n"
#define MSGTR_AO_ALSA_UnableToSetAccessType "[AO_ALSA] Impossible de fixer le type d'accès : %s\n"
#define MSGTR_AO_ALSA_FormatNotSupportedByHardware "[AO_ALSA] Format %s non supporté par le matériel, essaie défaut.\n"
#define MSGTR_AO_ALSA_UnableToSetFormat "[AO_ALSA] Impossible de fixer le format : %s\n"
#define MSGTR_AO_ALSA_UnableToSetChannels "[AO_ALSA] Impossible de fixer le canal : %s\n"
#define MSGTR_AO_ALSA_UnableToDisableResampling "[AO_ALSA] Impossible de désactiver resampling : %s\n"
#define MSGTR_AO_ALSA_UnableToSetSamplerate2 "[AO_ALSA] Impossible de fixer samplerate-2 : %s\n"
#define MSGTR_AO_ALSA_UnableToSetBufferTimeNear "[AO_ALSA] Impossible de fixer le temps du tampon le plus poche : %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodTime "[AO_ALSA] Impossible de fixer la durée de la période : %s\n"
#define MSGTR_AO_ALSA_BufferTimePeriodTime "[AO_ALSA] Tampon/temps : %d, période/temps : %d\n"
#define MSGTR_AO_ALSA_UnableToGetPeriodSize "[AO ALSA] Obtention impossible de la grandeur de la période : %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodSize "[AO ALSA] Impossible de fixer la taille de la période(%ld) : %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriods "[AO_ALSA] Impossible de fixer les périodes : %s\n"
#define MSGTR_AO_ALSA_UnableToSetHwParameters "[AO_ALSA] Impossible de fixer hw-parameters : %s\n"
#define MSGTR_AO_ALSA_UnableToGetBufferSize "[AO_ALSA] Obtention impossible de la taille du tampon : %s\n"
#define MSGTR_AO_ALSA_UnableToGetSwParameters "[AO_ALSA] Obtention impossible de sw-parameters : %s\n"
#define MSGTR_AO_ALSA_UnableToSetSwParameters "[AO_ALSA] Impossible de fixer sw-parameters : %s\n"
#define MSGTR_AO_ALSA_UnableToGetBoundary "[AO_ALSA] Obtention impossible de la limite : %s\n"
#define MSGTR_AO_ALSA_UnableToSetStartThreshold "[AO_ALSA] Impossible de fixer le seuil de départ : %s\n"
#define MSGTR_AO_ALSA_UnableToSetStopThreshold "[AO_ALSA] Impossible de fixer le seuil d'arrêt : %s\n"
#define MSGTR_AO_ALSA_UnableToSetSilenceSize "[AO_ALSA] Impossible de fixer la grandeur du silence : %s\n"
#define MSGTR_AO_ALSA_PcmCloseError "[AO_ALSA] Erreur fermeture pcm : %s\n"
#define MSGTR_AO_ALSA_NoHandlerDefined "[AO_ALSA] Aucun gestionnaire (handler) défini !\n"
#define MSGTR_AO_ALSA_PcmPrepareError "[AO_ALSA] Erreur préparation pcm : %s\n"
#define MSGTR_AO_ALSA_PcmPauseError "[AO_ALSA] Erreur pause pcm : %s\n"
#define MSGTR_AO_ALSA_PcmDropError "[AO_ALSA] Erreur drop pcm : %s\n"
#define MSGTR_AO_ALSA_PcmResumeError "[AO_ALSA] Erreur reprise pcm : %s\n"
#define MSGTR_AO_ALSA_DeviceConfigurationError "[AO_ALSA] Erreur configuration périphérique."
#define MSGTR_AO_ALSA_PcmInSuspendModeTryingResume "[AO_ALSA] Pcm en mode suspendu, essaie de relancer.\n"
#define MSGTR_AO_ALSA_WriteError "[AO_ALSA] Erreur en écriture : %s\n"
#define MSGTR_AO_ALSA_TryingToResetSoundcard "[AO_ALSA] Essaie de réinitialiser la carte son.\n"
#define MSGTR_AO_ALSA_CannotGetPcmStatus "[AO_ALSA] Ne peux avoir le statut pcm : %s\n"


// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] plugiciel invalide : %s\n"

// ======================= AF Filtres Audio ================================

// libaf 
#define MSGTR_AF_ValueOutOfRange MSGTR_VO_ValueOutOfRange

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "Labels disponibles dans"
#define MSGTR_AF_LADSPA_WarnNoInputs "AVERTISSEMENT ! Plugin LADSPA sans entrée audio.\n  Le signal entrée audio sera perdu."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Plugins multi-canal (>2) non (encore) supportés.\n  Utiliser plugins mono ou stéréo."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Plugin LADSPA sans sortie audio."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Désaccord entre le nombre d'entrées et de sorties audio du plugin LADSPA."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "Echec de chargement"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Fonction ladspa_descriptor() introuvable dans fichier lib spécifié."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Label introuvable dans lib du plugin."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Aucune sous-option spécifiée"
#define MSGTR_AF_LADSPA_ErrNoLibFile "Aucun fichier lib spécifié"
#define MSGTR_AF_LADSPA_ErrNoLabel "Aucun label de filtre spécifié"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Pas assez de contrôles spécifiés sur ligne de commande"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s : Contrôle d'entrée #%d sous limite inférieure de %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s : Contrôle d'entrée #%d sous limite supérieure de %0.4f.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "format inconnu"

// ========================== ENTREE =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "Ouverture périphérique manette de jeux %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Impossible d'ouvrir périphérique manette de jeux %s : %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Erreur lecture périphérique manette de jeux : %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Manette de jeux : perdons %d bytes de données\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Manette de jeux : alerte événement init, perte de sync avec pilote\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Alerte manette de jeux événement inconnu de type %d\n"

// appleir.c

#define MSGTR_INPUT_APPLE_IR_Init "Initialisation de l'interface IR Apple sur %s\n"
#define MSGTR_INPUT_APPLE_IR_Detect "Interface IR Apple détectée sur %s\n"
#define MSGTR_INPUT_APPLE_IR_CantOpen "Impossible d'ouvrir l'interface IR Apple : %s\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Trop de descripteurs de fichiers de commande. Impossible d'enregister descripteur fichier %d.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Trop de descripteurs de fichiers touche. Impossible d'enregister descripteur fichier %d.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "Commande %s : argument %d pas un nombre entier.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "Commande %s : argument %d pas un nombre réel.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "Commande %s : argument %d non terminé.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Argument inconnu %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "Commande %s requiert au moins %d arguments, trouvé seulement %d.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Erreur lecture descripteur fichier commande %d : %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "Tampon de commande du descripteur de fichier %d plein : omet contenu\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Commande invalide pour touche liée %s"
#define MSGTR_INPUT_INPUT_ErrSelect "Erreur de sélection : %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Erreur sur descripteur de fichier entré touche %d\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Entré couche morte sur descripteur fichier %d\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Trop événements touche appuyé en même temps\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Erreur sur descripteur fichier commande %d\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Erreur lecture fichier config entré %s : %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Clé inconnue '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Liaison non terminée %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "Tampon trop petit pour nom de touche : %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "Aucune commande trouvée pour touche %s"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "Tampon trop petit pour commande %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "Que faisons-nous ici ?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "Impossible d'initier manette entrée\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "Impossible lire %s : %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Impossible ouvrir %s : %s\n"
#define MSGTR_INPUT_INPUT_ErrCantInitAppleRemote "Impossible d'initier télécommande Apple Remote.\n"

// ========================== LIBMPDEMUX ===================================

// url.c

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "La chaîne semble déjà échappée dans url_escape %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Impossible de régler taux échantillon\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Impossible de régler heure tampon\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Impossible de régler heure période\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Configuration brisée pour ce PCM : aucune configuration disponible\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Type d'accès non disponible\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Format d'échantillon non disponible\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Compte de canaux non dispo - retour à valeur défaut : %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Impossible d'installer les paramètres matériels : %s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Impossible d'utiliser période égale à grandeur tampon (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Impossible d'installer les paramètres logiciels :n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Erreur ouverture audio : %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "Erreur statut ALSA : %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun !!! (au moins %.3f ms long)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "Statut ALSA :\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: erreur préparation : %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "Erreur lecture/écriture ALSA"

// ai_oss.c

#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Impossible mettre compte canaux : %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Impossible de mettre en stéréo : %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "Impossible d'ouvrir '%s' : %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "Format non supporté\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Impossible de mettre le format audio."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "Impossible de fixer taux d'échantillon : %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "Impossible de mettre le déclencheur: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Impossible d'obtenir grandeur bloc !\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "Grandeur bloc audio zéro, met à %d !\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "Grandeur bloc audio trop bas, met à %d !\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "FATAL : grandeur de l'entête supérieure à 1 MB (%d)!\nContacter auteurs MPlayer, et télécharger/envoyer ce fichier.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Impossible d'allouer %d octets pour l'entête\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "EOF en lisant entête asf, fichier brisé/incomplet ?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR pourrait fonctionner seulement avec libavformat, en cas problème, essayer -demuxer 35\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Nul morceau données suit entête !\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF : nul entête audio ou vidéo trouvé - fichier brisé ?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Longueur entête ASF invalide !\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMLicenseURL "URL de la license DRM : %s\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMProtected "Ce fichier est entaché d'une chiffrage DRM.  Il n'est pas lisible avec MPlayer!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "Erreur écriture\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nAlerte! EOF\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "Échec lecture pré-entête\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Grandeur entête invalide, abandon\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Échec lecture données entête\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "Échec lecture packet_len\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Grandeur paquet rtsp invalide, abandon\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Échec lecture données commande\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "Objet entête\n"
#define MSGTR_MPDEMUX_MMST_DataObject "Objet données\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "Objet fichier, longueur paquet = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "Objet flux, id flux : %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Trop de ID, flux sauté"
#define MSGTR_MPDEMUX_MMST_UnknownObject "Objet inconnu\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Échec lecture données média\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "Signature manquante\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Tout est fait. Merci pour le téléchargement de fichier contenant technologie propriétaire et patenté.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "commande inconnue %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "erreur get_media_packet : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Connecté\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, grandeur bloc flux trop petite : %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "désaccord confirme_grandeur ! : %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Alerte : omet entête ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Échec analyse entête morceau\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Aucune entête comme premier morceau !!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Erreur : ne peux allouer tampon %d octets\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Erreur lecture flux réseau\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Erreur morceau trop petit\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Erreur nombre sous-morceaus invalide\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Bande passante trop petite, ne peux lire fichier !\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "Bande passante trop petite, flux audio désélectionné\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "Bande passante trop petite, flux vidéo désélectionné\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Longueur entête ASF invalide !\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Erreur lecture entête morceau\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Erreur grandeur morceau > grandeur paquet\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Erreur lecture morceau\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> Redirecteur ASF\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "Proxy URL invalide\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "Genre de flux asf inconnu\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Échec analyse réponse HTTP\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Retour de serveur %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ALERTE ANALYSE ASF HTTP : Pragma %s coupé de %d octets à %d\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Erreur lecture interface (socket) : %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Échec analyse entête\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Aucun flux trouvé\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "Type de flux ASF inconnu\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL : %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Échec, abandon\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nErreur lecture audio : %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Rétabli de cross-run, quelques images pourraient être exclues !\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Erreur fatale : impossible de se rétablir !\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nNombre insuffisant d'échantillons audio !\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** Liste vide ?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Film trouvé à 0x%X - 0x%X\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "'BIH' trouvé, %u octets de %d\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "Regeneration de table image clé pour vidéo M$ mpg4v1\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Regeneration de table image clé pour DIVX3 video\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Regeneration de table image clé pour MPEG4 video\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "'WF' trouvé, %d octets de %d\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI : dmlh trouvé (grandeur=%d) (total images=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "Lecture morceau INDEX, %d morceaux pour %d images (fpos=%"PRId64")\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "Entête RIFF additionnel...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** alerte : aucun entête AVI étendu...\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Morceau brisé ?  Grandeur morceau=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI : ODML : Construction index odml (%d super moreaux index)\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI : ODML : Fichier brisé (incomplet ?) détecté. Utilise index traditionnel\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "Impossible de lire fichier index %s : %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s n'est pas un fichier index MPlayer valide\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Impossible d'allouer mémoire pour données index de %s\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "Fin de fichier index inattendue %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Fichier index chargé : %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Génération Index : %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI : table index générée pour %d morceaux !\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Impossible d'écrire le fichier index %s : %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Sauvegardé fichier index : %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rFlux non enrégistrable en mémoire cache.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! diff lecture position fichier !!! rapporter ce bogue...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Impossible ouvrir périphérique CDDA.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Impossible ouvrir disque.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "CD audio trouvé avec %ld pistes.\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Échec lecture TDM.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Échec d'ouverture du périphérique %s.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "URL non valide\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "Échec envoie requète HTTP.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "Échec lecture réponse HTTP.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Non trouvé.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "Code erreur inconnu\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Aucun tampon trouvé.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Lecture incomplète de fichier xmcd.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Échec création répertoire %s.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Écriture incomplète de fichier xmcd.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "Retour invalide de fichier base de données xmcd.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "FIXME inattendu\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "Code non géré\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Impossible trouver fin de ligne\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Analyse OK, trouvé : %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album non trouvé\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Réponse serveur : Erreur syntaxe commande\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Aucune information sites disponible\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Échec obtention niveau de protocol\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "Aucun CD dans lecteur\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Ligne de fichier signal inattendue : %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] nom fichier bin testé : %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Impossible de trouver fichier bin - abandon\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] Utilise fichier bin %s\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] Mode inconnu pour fichier bin. Improbable. Fin.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] Impossible d'ouvrir %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Erreur lecture %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Erreur lecture grandeur fichier bin\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "Piste %02d :  format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] Fin inattendue de fichier bin\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Impossible de lire %d octets de données\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "Signal flux ouvert, nom fichier=%s, piste=%d, pistes disponibles : %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "Famille d'adresses inconnue %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "Solution de %s pour %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Impossible de trouver nom pour %s : %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Connexion au serveur %s[%s] : %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Échec connexion au serveur avec %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Échec sélection.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "Dépassement de temps pour connecter.\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "Échec getsockopt : %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "Erreur de connection : %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Réglage proxy invalide... Essaie sans proxy.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Impossible résoudre nom hôte distant pour AF_INET. Essaie sans proxy.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Erreur lors envoie requète HTTP: envoie incomplêt.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Échec lecture.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response = 0 (i.e. EOF)\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Échec Authentification. Utiliser les options -user et -passwd pour donner votre\n"\
"nom_utilisateur/mot_de_passe pour une liste de URLs, ou donner une URL tel que :\n"\
"http://nom_utilisateur:mot_de_passe@nom_hôte/fichier\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Authentification requise pour %s\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Authentification requise.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Aucun mot_de_passe fourni, essaie mot_de_passe vide.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Serveur retourne %d : %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Grandeur cache réglée à %d KBytes\n"

// demux_audio.c

#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "Demuxer audio : format inconnu %d.\n"

// demux_demuxers.c

#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "Erreur fill_buffer : Mauvais demuxer : pas de vd, ad ou sd.\n"

// demux_mkv.c
#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] Echec initialisation de zlib.\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] Echec décompression de zlib.\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] Echec initialisation de lzo.\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] Echec décompression de lzo.\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] Le n° de piste %u a été encrypté et le décryptage n'a pas encore été\n[mkv] mis en place. Saut de piste.\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] Unknown content encoding type for track %u. Saut de piste.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] Piste %u a été compressée avec un algorithme de comrpession (%u)\n[mkv] inconnu/non supporté. Saut de piste.\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] Piste %u a été compressée avec zlib mais MPlayer n'a pas été compilé\n[mkv] avec le support de compression pour zlib. Saut de piste.\n"
#define MSGTR_MPDEMUX_MKV_TrackIDName "[mkv] Ident. piste %u : %s (%s) \"%s\", %s\n"
#define MSGTR_MPDEMUX_MKV_TrackID "[mkv] Ident. piste %u : %s (%s), %s\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] Ident. codec inconnue/non supportée (%s) ou données manquantes/mauvais codec privé\n[mkv] (piste %u).\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] Piste FLAC ne contient pas d'entêtes valides.\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv] Ident. codec audio '%s' inconnu/non supporté  pour piste %u ou \n[mkv]données sur codec privé manquantes/erronées.\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] Type de sous-titre '%s' non supporté.\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] Jouera piste vidéo %u.\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] Pas de piste vidéo trouvée/voulue.\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] Pas de piste audio trouvée/voulue.\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] Affichera piste sous-titre %u.\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] Attention : aucun \"BlockDuration\" pour piste sous-titre trouvé.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Attention : trop de \"sublines\" à restituer, passe.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] Attention : trop de \"sublines\" à restituer, passe après premier %i.\n"

// demux_nuv.c

#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "Pas de blocs video dans le fichier.\n"

// demux_xmms.c

#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Plugin trouvé : %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Fermeture du plugin : %s.\n"
#define MSGTR_MPDEMUX_XMMS_WaitForStart "Attente de fermeture du greffon XMMS pour démarrer la lecture de '%s'...\n"

// ========================== LIBMPMENU ===================================

// common

#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] Aucune entrée trouvée dans la définition du menu.\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] Erreur syntaxe ligne : %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] Définitions de menu exigent attribut de nom (ligne %d)\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] Mauvais attribut %s=%s dans menu '%s', ligne %d\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] Genre menu inconnu '%s' ligne %d\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] Ne peux ouvrir fichier config menu : %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] Fichier config trop gros (> %d KO)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] Fichier config vide\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] Menu %s non trouvé.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] Menu '%s' : échec init\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] Format de sortie non supporté !!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] Besoin de nom pour définitions entrée menu liste (ligne %d).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] Menu liste exige argument.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Erreur attente identificateur processus : %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Erreur sélection.\n"
#define MSGTR_LIBMENU_ReadErrorOnChildFD "[MENU] Erreur lecture sur processus enfant : %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] Console run : %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] Processus enfant déjà en cours.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Échec branchement !!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] Erreur écriture.\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] Erreur ouverture répertoire : %s.\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] Erreur réallocation mémoire : %s.\n"
#define MSGTR_LIBMENU_MallocError "[MENU] Erreur allocation mémoire : %s.\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] Erreur lecture répertoire : %s.\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] Ne peux ouvrir répertoire %s\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] Définition sous-menu exige attribut 'menu'.\n"
#define MSGTR_LIBMENU_InvalidProperty "[MENU] Propriété invalide '%s' dans l'entrée menu pref. (line %d).\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] Définition entrée menu pref exige attribut 'propriété' valide (ligne %d).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] Menu pref exige argument.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] Ne peux trouver item cible ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] Echec composition commande : %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] Menu texte exibe nom fichier txt (fichier param).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] Ne peux ouvrir : %s.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] Alerte, ligne trop longue. Je la coupe.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] %d lignes analysées.\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] Commande inconnue : '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] Échec ouverture menu : '%s'.\n"

// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] Alerte ! Différence grandeur trame audio ! lu=%d  hdr=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Impossible d'allouer image pour codec cinepak.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] codec accéléré XVMC .\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] Moyenne arithmétique de QP : %2.4f, moyenne harmonique de QP : %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] Échec DRI.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Impossible d'allouer image pour codec.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] MPEG2 accéléré XVMC.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] Essaie pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] Le mc_get_buffer devrait fonctionner seulement avec accélération XVMC !!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Erreur init_vo inattendue.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Erreur fatale, tampons de rendement non pris.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Seuls les tampons alloués par vo_xvmc permis.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] Codage haute qualité sélectionné (non temps réel) !\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] Utilise qscale constant = %f (VBR).\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] Sortie brut avec fourcc [%x] non supporté !\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] Codec VfW requis non spécifié !!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] Mauvaise position/largeur/hauteur - aire coupée hors original !\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] Aire coupée: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] Nom de format inconnu : '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] Erreur transmission arguments.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "Genre compresseur : %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "Sous-genre compresseur : %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "Indicateurs de compresseur : %lu, version %lu, ICM version : %lu\n"
#define MSGTR_MPCODECS_Flags "Indicateurs :"
#define MSGTR_MPCODECS_Quality "Qualité"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "Plein DR impossible, essaie plutôt TRANCHES !\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "Alerte ! Filtre suivant ne supporte pas TRANCHES, gare au sig11...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "Pourquoi ce NULL ??\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "%s non supporté par filtre suivant/vo :(\n"

// ================================== LIBMPVO ====================================

// mga_common.c

#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] Erreur dans mga_vid_config ioctl (mauvaise version de mga_vid.o ?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] Impossible d'avoir les valeurs de luma depuis le module du noyau !\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] Impossible de fixer les valeurs de luma depuis le module du noyau !\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] Largeur/hauteur écran inconnue !\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] Format de sortie invalide %0X\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] La version de votre pilote mga_vid est incompatible avec cette version de MPlayer !\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] Impossible d'ouvrir : %s\n"
#define MSGTR_LIBVO_MGA_ResolutionTooHigh "[MGA] La resolution à sa source est au moins dans une dimension plus large que 1023x1023. Veuillez remettre à l'échelle dans le logiciel ou utiliser -lavdopts lowres=1\n"
#define MSGTR_LIBVO_MGA_mgavidVersionMismatch "[MGA] La version du driver mga_vid (%u) ne correspond pas à celle utilisée lors de la compilation de MPlayer (%u)\n"

// libvo/vesa_lvo.c

#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] Cette branche n'est plus supportée.\n[VESA_LVO] Veuillez plutôt utiliser -vo vesa:vidix.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] Impossible d'ouvrir : '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] Format de sortie invalide : %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] La version de votre pilote fb_vid est incompatible avec cette version de MPlayer !\n"

// libvo/vo_3dfx.c

#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] 16bpp uniquement supporté !"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] ID visuel est  %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] Impossible d'ouvrir /dev/3dfx.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] Erreur : %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] Impossible de cartographier l'aire mémoire 3dfx : %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] Initialisé : %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] Sous-périphérique inconnu : %s.\n"

// libvo/aspect.c
#define MSGTR_LIBVO_ASPECT_NoSuitableNewResFound "[ASPECT] Attention : Pas de nouvelle résolution adéquate détectée !\n"
#define MSGTR_LIBVO_ASPECT_NoNewSizeFoundThatFitsIntoRes "[ASPECT] Erreur : Pas de nouvelle taille détectée qui fonctionne avec la résolution !\n"

// libvo/vo_dxr3.c

#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] Impossible de charger la nouvelle palette SPU !\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] Impossible de mettre en mode lecture !\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] Impossible de mettre le mode subpicture !\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] Impossible d'avoir un norme TV !\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] Norme TV auto-sélectionnée par le nombre d'images par seconde : "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] Impossible de mettre la norme TV !\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] Réglé pour NTSC.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] Réglé pour PAL/SECAM.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] Format d'image réglé sur 4:3.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] Format d'image réglé sur 16:9.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] Plus de mémoire\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] Impossible d'allouer clé colorimétrique !\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] Impossible d'allouer clé colorimétrique exacte, utilise la valeur la plus proche (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] Dé-initialisation.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] Echec de restauration de la norme TV !\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] Activation du pré-tampon.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] Utilise un nouveau moteur de sync.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] Utilise superposition.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] Erreur : Superposition requiert compilation avec X11 libs/indexes installés\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] Norme TV sera fixée à : "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "Auto-ajustement au nombre d'images par second du film (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "Auto-ajustement au nombre d'images par second du film (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "Utilise norme actuelle."
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "Norme fournie inconnue. Utilise norme actuelle."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] Erreur ouverture %s en écriture, essaie plutôt /dev/em8300.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] Erreur ouverture %s en écriture, essaie plutôt /dev/em8300_mv.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] Erreur ouverture /dev/em8300 en écriture également !\nSorti d'affaire.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] Erreur ouverture /dev/em8300_mv en écriture également !\nSorti d'affaire.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] Ouvert : %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] Erreur ouverture %s en écriture, essaie plutôt /dev/em8300_sp.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] Erreur ouverture /dev/em8300_sp en écriture également !\nSorti d'affaire.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] Ouverture impossible  de l'affichage pendant l'établissement du hack overlay !\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] Initialisation de X11 impossible !\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] Echec réglage attribut superposition.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] Echec réglage écran superposition !\nQuitte.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] Echec activation superposition !\nQuitte.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] Echec du redimentionnement de fenêtre superposée !\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] Echec réglage superposition bcs !\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] Echec obtention des valeurs de décallage Y de superposition !\nQuitte.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] Echec obtention des valeurs de décallage X de superposition !\nQuitte.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] Echec obtention des corrections d'échelle X de superposition !\nQuitte.\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Décallage Y : %d.\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] Décallage X : %d.\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] Correction X : %d.\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] Echec réglage du mix signal !\n"

// libvo/font_load_ft.c

#define MSGTR_LIBVO_FONT_LOAD_FT_NewFaceFailed "New_Face a échoué.  Le chemin vers les fonts est peut-être faux.\nSpécifiez un fichier de police, svp. (~/.mplayer/subfont.ttf).\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_NewMemoryFaceFailed "New_Memory_Face a échoué.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_SubFaceFailed "Police de sous-titres : load_sub_face a échoué.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_SubFontCharsetFailed "Police de sous-titres : prepare_charset a échoué.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareSubtitleFont "Impossible de préparer la police de sous-titres.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareOSDFont "Impossible de prparer la police OSD.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotGenerateTables "Impossible de générer les tables.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_DoneFreeTypeFailed "FT_Done_FreeType a échoué.\n"

// libvo/vo_mga.c

#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect() : redimensionné à %dx%d.\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] Dé-initialisation !\n"

// libvo/vo_null.c

#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] Sous-périphérique inconnu : %s.\n"

// libvo/vo_png.c

#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] Alerte : Niveau de compression fixé à 0, compression désactivée !\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] Info : utilisez -vo png:z=<n> pour fixer le niveau de compression de 0 à 9.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] Info : (0 = pas de compression, 1 = plus rapide, plus basse - 9 meilleur, compression plus lente)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] Erreur ouverture '%s' en écriture !\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] Erreur dans create_png.\n"

// libvo/vo_sdl.c

#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] Impossible d'avoir un mode SDL acceptable en sortie.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode : Echec SDL_SetVideoMode : %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode : Echec SDL_SetVideoMode : %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] Cartographie I420 à IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] Format d'image non supporté (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] Info - veuillez utiliser -vm ou -zoom pour permuter vers la meilleure résolution.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] Impossible de fixer mode vidéo : %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] Impossible de créer une superposition YUV : %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] Impossible de créer une surface RGB : %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] Utiliser la conversion profondeur/espace colorimétrique, va ralentir le processus (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] Format d'images non supporté dans draw_slice, veuillez contacter les développeurs de MPlayer !\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Echec Blit : %s.\n"
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] Echec initialisation SDL : %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] Utilisation du pilote : %s.\n"

// libvo/vobsub_vidix.c

#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] Lancement lecture impossible : %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] Arrêt lecture impossible : %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] UV entrelacé pour YUV410P non supporté.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] vidix_draw_slice() factice appelé.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] vidix_draw_frame() factice appelé.\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] FourCC non supporté pour ce pilote VIDIX : %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] Serveur vidéo a une résolution non supportée (%dx%d), supportée : %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] Serveur vidéo a une profondeur de couleur non supportée par VIDIX (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] Le pilote VIDIX ne peut agrandir image (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] Le pilote VIDIX ne peut réduire image (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] Configuration de la lecture impossible : %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] Vous avez une mauvaise version de la lib VIDIX.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] Impossible de trouver un pilote VIDIX qui marche.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] Obtention de la capabilité impossible : %s.\n"

// libvo/vo_svga.c

#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] vid_mode forcé %d (%s) non disponible.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] vid_mode forcé %d (%s) trop petit.\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode : %d, %dx%d %dbpp.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Echec Vga_setmode(%d).\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] Le mode vidéo est linéaire et memcpy peut être utilisé pour le transfert d'images.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] Le mode vidéo dispose de l' accélération matériel et put_image pourrait être utilisé.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] Si cela marche chez vous j'aimerais le savoir. \n[VO_SVGA] (envoyer le log avec `mplayer test.avi -v -v -v -v &> svga.log`). Merci !\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] Le mode vidéo contient %d page(s).\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] Centrer image. Commencer à (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] Utilise VIDIX. w=%i h=%i  mw=%i mh=%i\n"

// libvo/vo_tdfxfb.c

#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] Ne peux ouvrir %s : %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] Problème avec FBITGET_FSCREENINFO ioctl : %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] Problème avec FBITGET_VSCREENINFO ioctl : %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] Ce pilote ne supporte que les 3Dfx Banshee, Voodoo3 and Voodoo 5.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] Sortie %d bpp non supportée.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] Impossible de cartographier aire mémoire : %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] Sortie %d bpp non supportée (Cela n'aurait jamais dû se produire).\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Eik ! Un problème avec control().\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] Pas assez de mémoire vidéo pour lire ce film. Essayez avec une résolution plus basse.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] Ecran est %dx%d à %d bpp, in est %dx%d à %d bpp, norme est %dx%d.\n"

// libvo/vo_tdfx_vid.c

#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Deplace %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] Echec déplacement AGP pour dégager l'écran.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Echec Blit.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] Format de superposition non-natif demande une conversion.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] Format d'entrée non supporté 0x%x.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Echec installation superposition.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Echec superposition activée.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Superposition prête : %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] Texture blit prête : %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Echec superposition désactivée\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] Ne peux ouvrir %s : %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] Ne peux avoir la configuration actuelle : %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Echec Memmap !!!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "Get image todo.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] Echec déplacement AGP.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] Echec application YUV.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] Echec déplacement AGP sur plan Y.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] Echec déplacement AGP sur plan U.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] Echec déplacement AGP sur plan V.\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] Format inconnu : 0x%x.\n"

// libvo/vo_tga.c

#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] Sous-périphérique inconnu : %s.\n"

// libvo/vo_vesa.c

#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] Erreur fatale produite ! Impossible de continuer.\n"
#define MSGTR_LIBVO_VESA_UnknownSubdevice "[VO_VESA] Sous-périphérique inconnu : '%s'.\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] Pas assez de mémoire vidéo pour ce mode :\n[VO_VESA] Requis : %08lX présent : %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] Spécifiez les capacités du moniteur. Ne pas changer le taux de rafraîchissement.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] Mode non adapté aux limites du moniteur. Ne pas changer le taux de rafraîchissement.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] Erreur fatale interne détectée : init est appelé avant preinit.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] Option -flip non supportée.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] Raison possible : pas de VBE2 BIOS.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] Version VESA VBE BIOS trouvée %x.%x Révision : %x.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] Mémoire vidéo : %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] Capacités VESA : %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! Infos OEM imprimées ci-dessous !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] Vous devriez voir 5 lignes en relation avec OEM ; sinon, vous avez cassé vm86.\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] Info OEM : %s.\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] Révision OEM : %x.\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] Vendeur OEM : %s.\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] Nom du produit OEM : %s.\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] Révision produit OEM : %s.\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] Indice : Pour que la sortie TV fonctionne vous devriez avoir branché le connecteur TV\n"\
"[VO_VESA] avant de démarrer puisque VESA BIOS est initialisé seulement durant POST.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] Utilisation du mode VESA (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] Ne peux initialiser le redimensionnement logiciel.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] Ne peux utiliser DGA. Force le mode bank switching. :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] Utilise DGA (ressources physiques : %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] Ne peux utiliser le double tampon : pas assez de mémoire vidéo.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] Ne trouve ni DGA ni le cadre de fenêtre repositionable.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] Vous avez forcé DGA. Sortie\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] Ne peux trouver une adresse de fenêtre valide.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] Utilise le mode bank switching(ressources physiques : %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] Ne peux allouer de tampon temporaire.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] Désolé, mode non supporté -- essayez -x 640 -zoom.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] Oh vous avez vraiment une image sur la TV !\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] Ne peux initialiser la superposition vidéo Linux.\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] Utilise la superposition vidéo : %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] Ne peux initialiser le pilote VIDIX.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] Utilise VIDIX.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] Ne peux trouver un mode pour : %ux%u@%u.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] Initialisation VESA terminée.\n"

// libvo/vo_x11.c

#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] draw_frame() appelé !!!!!!\n"

// libvo/vo_xv.c

#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] draw_frame() appelé !!!!!!\n"
#define MSGTR_LIBVO_XV_SharedMemoryNotSupported "[VO_XV] Mémoire partagée non supportée\nRetour  vers normal Xv.\n"
#define MSGTR_LIBVO_XV_XvNotSupportedByX11 "[VO_XV] Désolé, Xv non supporté par cette version X11/pilote\n[VO_XV] ******** Essayez avec  -vo x11  ou  -vo sdl  *********\n"
#define MSGTR_LIBVO_XV_XvQueryAdaptorsFailed  "[VO_XV] Echec XvQueryAdaptors.\n"
#define MSGTR_LIBVO_XV_InvalidPortParameter "[VO_XV] Paramètre du port invalide, passe outre avec port 0.\n"
#define MSGTR_LIBVO_XV_CouldNotGrabPort "[VO_XV] Ne peux saisir le port %i.\n"
#define MSGTR_LIBVO_XV_CouldNotFindFreePort "[VO_XV] Ne peux pas trouver de port Xvideo libre - Un autre process utilise peut-être \n"\
"[VO_XV] ce port. Fermez les applications vidéo et essayez encore. Si rien n'y \n"\
"[VO_XV] fait, voir 'mplayer -vo help' pour autres pilotes sortie vidéo (non xv) .\n"
#define MSGTR_LIBVO_XV_NoXvideoSupport "[VO_XV] Apparemment, aucun support Xvidéo disponible pour votre carte vidéo.\n"\
"[VO_XV] Lancez 'xvinfo' pour vérifier son support Xv et lire\n"\
"[VO_XV] DOCS/HTML/en/video.html#xv!\n"\
"[VO_XV] Voir 'mplayer -vo help' pour autres pilotes sortie vidéo (non xv).\n"\
"[VO_XV] Essayez -vo x11.\n"


// loader/ldt_keeper.c

#define MSGTR_LOADER_DYLD_Warning "AVERTISSEMENT : Tentative d'utilisation de codecs DLL alors que la variable d'environment\n         DYLD_BIND_AT_LAUNCH n'est pas assignée. Plantage très probable.\n"

// stream/stream_radio.c

#define MSGTR_RADIO_ChannelNamesDetected "[radio] Noms de canal radio detectés.\n"
#define MSGTR_RADIO_FreqRange "[radio] La plage de fréquence permise est %.2f-%.2f MHz.\n"
#define MSGTR_RADIO_WrongFreqForChannel "[radio] Mauvaise fréquence pour canal %s\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[radio] Mauvais n° de canal : %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[radio] Mauvais n° de canal : %d\n"
#define MSGTR_RADIO_WrongChannelName "[radio] Mauvais nom de canal : %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[radio] Paramètre de fréquence radio detecté.\n"
#define MSGTR_RADIO_DoneParsingChannels "[radio] Analyse des canaux effectuée.\n"
#define MSGTR_RADIO_GetTunerFailed "[radio] Attention : ioctl, échec de tuner : %s. Ajustement de frac à %d.\n"
#define MSGTR_RADIO_NotRadioDevice "[radio] %s : pas de périphérique radio !\n"
#define MSGTR_RADIO_TunerCapLowYes "[radio] Tuner est bas : oui frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[radio] tuner est bas : non frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[radio] Echec ioctl fixe la fréquence 0x%x (%.2f) : %s\n"
#define MSGTR_RADIO_GetFreqFailed "[radio] Echec ioctl récupère fréquence : %s\n"
#define MSGTR_RADIO_SetMuteFailed "[radio] Echec ioctl mise en muet : %s\n"
#define MSGTR_RADIO_QueryControlFailed "[radio] Echec contrôle de requête ioctl : %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[radio] Echec ioctl récupère le volume : %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[radio] Echec ioctl met le volume: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[radio] Dommage - perte de frame audio (%d bytes) !\n"
#define MSGTR_RADIO_BufferEmpty "[radio] grab_audio_frame : tampon vide, attente de %d bytes de données.\n"
#define MSGTR_RADIO_AudioInitFailed "[radio] Echec audio_in_init : %s\n"
#define MSGTR_RADIO_AudioBuffer "[radio] Capture audio - tampon=%d bytes (bloc=%d bytes).\n"
#define MSGTR_RADIO_AllocateBufferFailed "[radio] Ne peux allouer de tampon audio (bloc=%d,buf=%d) : %s\n"
#define MSGTR_RADIO_CurrentFreq "[radio] Fréquence actuelle : %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[radio] Canal sélectionné : %d - %s (fréq : %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[radio] Ne peux changer de canal : Aucune liste de canals donnée.\n"
#define MSGTR_RADIO_UnableOpenDevice "[radio] Impossible d'ouvrir '%s': %s\n"
#define MSGTR_RADIO_RadioDevice "[radio] Radio fd : %d, %s\n"
#define MSGTR_RADIO_InitFracFailed "[radio] Echec init_frac.\n"
#define MSGTR_RADIO_WrongFreq "[radio] Mauvaise fréquence : %.2f\n"
#define MSGTR_RADIO_UsingFreq "[radio] Utilise fréquence : %.2f.\n"
#define MSGTR_RADIO_AudioInInitFailed "[radio] Echec audio_in_init.\n"
#define MSGTR_RADIO_BufferString "[radio] %s : en tampon=%d perdu=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[radio] Echec appel audio_in_setup : %s\n"
#define MSGTR_RADIO_CaptureStarting "[radio] Début de la capture.\n"
#define MSGTR_RADIO_ClearBufferFailed "[radio] Echec effacement du tampon : %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[radio] Echec appel stream_enable_cache : %s\n"
#define MSGTR_RADIO_DriverUnknownStr "[radio] Nom de pilote inconnu : %s\n"
#define MSGTR_RADIO_DriverV4L2 "[radio] Utilise interface radio V4Lv2.\n"
#define MSGTR_RADIO_DriverV4L "[radio] Utilise interface radio V4Lv1.\n"
#define MSGTR_RADIO_DriverBSDBT848 "[radio] Utilisation de l'interface radio *BSD BT848.\n"
#define MSGTR_RADIO_AvailableDrivers "[radio] Drivers disponibles : "

// ================================== LIBASS ====================================

// ass_bitmap.c
#define MSGTR_LIBASS_FT_Glyph_To_BitmapError "[ass] Erreur FT_Glyph_To_Bitmap %d \n"
#define MSGTR_LIBASS_UnsupportedPixelMode "[ass] Mode pixel non supporté : %d\n"
#define MSGTR_LIBASS_GlyphBBoxTooLarge "[ass] Cadre du caractère trop grand: %dx%dpx\n"

// ass.c
#define MSGTR_LIBASS_NoStyleNamedXFoundUsingY "[ass] [%p] Avertissement: aucun style nommé '%s' trouvé, utilise '%s'\n"
#define MSGTR_LIBASS_BadTimestamp "[ass] mauvais marqueur de temps\n"


#define MSGTR_LIBASS_ErrorOpeningIconvDescriptor "[ass] erreur lors de l'ouverture du descripteur de conversion.\n"
#define MSGTR_LIBASS_ErrorRecodingFile "[ass] erreur lors de l'enregistrement du fichier.\n"
#define MSGTR_LIBASS_FopenFailed "[ass] ass_read_file(%s) : fopen a échoué\n"
#define MSGTR_LIBASS_FseekFailed "[ass] ass_read_file(%s) : fseek à échoué\n"
#define MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan10M "[ass] ass_read_file(%s) : Chargement des fichiers plus grands que 10Mo refusé\n"
#define MSGTR_LIBASS_ReadFailed "Lecture impossible, %d: %s\n"
#define MSGTR_LIBASS_AddedSubtitleFileMemory "[ass] Ajout d'un fichier de sous-titres : <memory> (%d styles, %d évènements)\n"
#define MSGTR_LIBASS_AddedSubtitleFileFname "[ass] Ajout d'un fichier de sous-titres : %s (%d styles, %d events)\n"
#define MSGTR_LIBASS_FailedToCreateDirectory "[ass] Impossible de créer le répertoire %s\n"
#define MSGTR_LIBASS_NotADirectory "[ass] Pas un répertoire : %s\n"

// ass_cache.c
#define MSGTR_LIBASS_TooManyFonts "[ass] Trop de polices de caractères\n"
#define MSGTR_LIBASS_ErrorOpeningFont "[ass] Erreur à l'ouverture de la police de caractère : %s, %d\n"

// ass_fontconfig.c
#define MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne "[ass] fontconfig : La police sélectionnée n'est pas celle demandée : '%s' != '%s'\n"
#define MSGTR_LIBASS_UsingDefaultFontFamily "[ass] fontconfig_select : Utilise la famille de police par defaut: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingDefaultFont "[ass] fontconfig_select : Utilise la police par defaut : (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingArialFontFamily "[ass] fontconfig_select : Utilise la famille de fonte 'Arial' : (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed "[ass] FcInitLoadConfigAndFonts a échoué.\n"
#define MSGTR_LIBASS_UpdatingFontCache "[ass] Mise à jour du cache des polices.\n"
#define MSGTR_LIBASS_BetaVersionsOfFontconfigAreNotSupported "[ass] Les versions Beta de fontconfig ne sont pas supportées.\n[ass] Effectuez une mise à jours avant de soumettre un rapport de bug.\n"
#define MSGTR_LIBASS_FcStrSetAddFailed "[ass] FcStrSetAdd a échoué.\n"
#define MSGTR_LIBASS_FcDirScanFailed "[ass] FcDirScan a échoué.\n"
#define MSGTR_LIBASS_FcDirSave "[ass] FcDirSave a échoué.\n"
#define MSGTR_LIBASS_FcConfigAppFontAddDirFailed "[ass] FcConfigAppFontAddDir a échoué\n"
#define MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed "[ass] Fontconfig desactivé, seule la police par defaut sera utilisée.\n"
#define MSGTR_LIBASS_FunctionCallFailed "[ass] %s a échoué\n"

// ass_render.c
#define MSGTR_LIBASS_NeitherPlayResXNorPlayResYDefined "[ass] Ni PlayResX, ni PlayResY ne sont définis. Suppose 384x288.\n"
#define MSGTR_LIBASS_PlayResYUndefinedSettingY "[ass] PlayResY non défini, ajuste à %d.\n"
#define MSGTR_LIBASS_PlayResXUndefinedSettingX "[ass] PlayResX non défini, ajuste à %d.\n"
#define MSGTR_LIBASS_FT_Init_FreeTypeFailed "[ass] FT_Init_FreeType a échoué.\n"
#define MSGTR_LIBASS_Init "[ass] Initialisation\n"
#define MSGTR_LIBASS_InitFailed "[ass] L'initialisation a échoué.\n"
#define MSGTR_LIBASS_BadCommand "[ass] Mauvaise commande: %c%c\n"
#define MSGTR_LIBASS_ErrorLoadingGlyph  "[ass] Erreur au chargement du caractère.\n"
#define MSGTR_LIBASS_FT_Glyph_Stroke_Error "[ass] Erreur FT_Glyph_Stroke %d \n"
#define MSGTR_LIBASS_UnknownEffectType_InternalError "[ass] Type d'erreur inconnu (erreur interne)\n"
#define MSGTR_LIBASS_NoStyleFound "[ass] Aucun style trouvé !\n"
#define MSGTR_LIBASS_EmptyEvent "[ass] Évènement vide !\n"
#define MSGTR_LIBASS_MAX_GLYPHS_Reached "[ass] MAX_GLYPHS atteint: évènement %d, début = %llu, durée = %llu\n Texte = %s\n"
#define MSGTR_LIBASS_EventHeightHasChanged "[ass] Avertissement! La hauteur de l'évènement a changé !\n"

// ass_font.c
#define MSGTR_LIBASS_GlyphNotFoundReselectingFont "[ass] Caractère 0x%X introuvable.  Sélectionne une police supplémentaire pour (%s, %d, %d)\n"
#define MSGTR_LIBASS_GlyphNotFound "[ass] Caractère 0x%X introuvable dans la police pour (%s, %d, %d)\n"
#define MSGTR_LIBASS_NoCharmaps "[ass] Famille de police sans description de table de caractères\n"
#define MSGTR_LIBASS_NoCharmapAutodetected "[ass] Pas de description de table de caractères détectée automatiquement.  Essai de la première\n"

//tv.c
#define MSGTR_TV_BogusNormParameter "tv.c: norm_from_string(%s) : paramètre de norme bogué.  Ajuste à %s.\n"
#define MSGTR_TV_NoVideoInputPresent "Erreur: Pas d'entrée vidéo présente!\n"
#define MSGTR_TV_UnknownImageFormat ""\
"==================================================================\n"\
" AVERTISSEMENT: FORMAT D'IMAGE DE SORTIE NON-TESTÉ OU INCONNU (0x%x)\n"\
" Ceci peut causer une lecture erronée ou un plantage ! Les rapports \n"\
" de bugs seront ignorés ! Vous devriez réessayer avec YV12 (l'espace \n"\
" de couleur par défaut) et lire la documentation !\n"\
"==================================================================\n"
#define MSGTR_TV_SelectedNormId "Identifiant de norme sélectionné: %d\n"
#define MSGTR_TV_SelectedNorm "Norme sélectionnée : %s\n"
#define MSGTR_TV_CannotSetNorm "Erreur : La norme ne peut pas être appliquée !\n"
#define MSGTR_TV_MJP_WidthHeight "  MJP: largeur %d hauteur %d\n"
#define MSGTR_TV_UnableToSetWidth "Impossible d'appliquer la largeur requise : %d\n"
#define MSGTR_TV_UnableToSetHeight "Impossible d'appliquer la hauteur requise : %d\n"
#define MSGTR_TV_NoTuner "L'entrée sélectionnée n'a pas de tuner !\n"
#define MSGTR_TV_UnableFindChanlist "Impossible de trouver la liste des canaux sélectionnés ! (%s)\n"
#define MSGTR_TV_SelectedChanlist "Liste des canaux sélectionnés: %s (contenant %d canaux)\n"
#define MSGTR_TV_ChannelFreqParamConflict "Il n'est pas possible de régler la fréquence et le canal simultanément !\n"
#define MSGTR_TV_ChannelNamesDetected "Noms des chaînes TV détectées.\n"
#define MSGTR_TV_NoFreqForChannel "Imposible de trouver la fréquence du canal %s (%s)\n"
#define MSGTR_TV_SelectedChannel3 "Canal sélectionné : %s - %s (fréq: %.3f)\n"
#define MSGTR_TV_SelectedChannel2 "Canal sélectionné : %s (fréq: %.3f)\n"
#define MSGTR_TV_SelectedFrequency "Fréquence sélectionnée : %lu (%.3f)\n"
#define MSGTR_TV_RequestedChannel "Canal choisi: %s\n"
#define MSGTR_TV_UnsupportedAudioType "Format audio '%s (%x)' non-supporté !\n"
#define MSGTR_TV_AudioFormat "  TV audio: %d canaux, %d bits, %d Hz\n"
#define MSGTR_TV_AvailableDrivers "Drivers disponibles:\n"
#define MSGTR_TV_DriverInfo "Driver sélectionné: %s\n nom : %s\n auteur : %s\n commentaire : %s\n"
#define MSGTR_TV_NoSuchDriver "Driver inexistant : %s\n"
#define MSGTR_TV_DriverAutoDetectionFailed "La détection auto du driver TV a échouée.\n"
#define MSGTR_TV_UnknownColorOption "Option couleur choisie inconnue (%d) !\n"
#define MSGTR_TV_CurrentFrequency "Fréquence actuelle : %lu (%.3f)\n"
#define MSGTR_TV_NoTeletext "Télétexte absent"
#define MSGTR_TV_Bt848IoctlFailed "tvi_bsdbt848: L'appel à %s ioctl a échoué. Erreur : %s\n"
#define MSGTR_TV_Bt848InvalidAudioRate "tvi_bsdbt848: Taux d'échantillonage audio invalide.  Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorOpeningBktrDev "tvi_bsdbt848: Impossible d'ouvrir le périphérique bktr.  Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorOpeningTunerDev "tvi_bsdbt848: Impossible d'ouvrir le périphérique tuner. Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorOpeningDspDev "tvi_bsdbt848: Impossible d'ouvrir le périphérique dsp. Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorConfiguringDsp "tvi_bsdbt848: La configuration du périphérique dsp a échoué. Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorReadingAudio "tvi_bsdbt848: Erreur de lecture des données audio.  Erreur : %s\n"
#define MSGTR_TV_Bt848MmapFailed "tvi_bsdbt848: mmap a échoué.  Erreur : %s\n"
#define MSGTR_TV_Bt848FrameBufAllocFailed "tvi_bsdbt848: L'allocation du buffer de trame a échoué.  Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorSettingWidth "tvi_bsdbt848: Erreur du réglage de la largeur de l'image. Erreur : %s\n"
#define MSGTR_TV_Bt848ErrorSettingHeight "tvi_bsdbt848: Erreur du réglage de la hauteur de l'image. Erreur : %s\n"
#define MSGTR_TV_Bt848UnableToStopCapture "tvi_bsdbt848: Impossible d'arréter la capture. Erreur : %s\n"
#define MSGTR_TV_TTSupportedLanguages "Langues supportées par le télétexte:\n"
#define MSGTR_TV_TTSelectedLanguage "Langue sélectionnée par defaut pour le télétexte : %s\n"
#define MSGTR_TV_ScannerNotAvailableWithoutTuner "Le scanner de canaux est indisponible sans le tuner\n"

//tvi_dshow.c
#define MSGTR_TVI_DS_UnableConnectInputVideoDecoder  "Impossible de connecter l'entrée spécifiée au décodeur vidéo. Erreur :0x%x\n"
#define MSGTR_TVI_DS_UnableConnectInputAudioDecoder  "Impossible de connecter l'entrée spécifiée au décodeur audio. Erreur :0x%x\n"
#define MSGTR_TVI_DS_UnableSelectVideoFormat "tvi_dshow: Impossible de sélectionner le format vidéo. Erreur :0x%x\n"
#define MSGTR_TVI_DS_UnableSelectAudioFormat "tvi_dshow: Impossible de sélectionner le format audio. Erreur :0x%x\n"
#define MSGTR_TVI_DS_UnableGetMediaControlInterface "tvi_dshow: Impossible de se connecter à une interface IMediaControl. Erreur :0x%x\n"
#define MSGTR_TVI_DS_DeviceNotFound "tvi_dshow: Périphérique #%d non trouvé\n"
#define MSGTR_TVI_DS_UnableGetDeviceName "tvi_dshow: Impossible de trouver un nom pour le périphérique #%d\n"
#define MSGTR_TVI_DS_UsingDevice "tvi_dshow: Utilise le périphérique #%d: %s\n"
#define MSGTR_TVI_DS_DeviceName  "tvi_dshow: Périphérique #%d: %s\n"
#define MSGTR_TVI_DS_DirectGetFreqFailed "tvi_dshow: Impossible d'obtenir la fréquence directement.  La table des canaux incluse à l'OS sera utilisée.\n"
#define MSGTR_TVI_DS_DirectSetFreqFailed "tvi_dshow: Impossible de fixer la fréquence directement.  La table des canaux incluse à l'OS sera utilisée.\n"
#define MSGTR_TVI_DS_SupportedNorms "tvi_dshow: normes supportées :"
#define MSGTR_TVI_DS_AvailableVideoInputs "tvi_dshow: Entrées vidéo disponibles :"
#define MSGTR_TVI_DS_AvailableAudioInputs "tvi_dshow: Entrées audio disponibles :"
//following phrase will be printed near the selected audio/video input
#define MSGTR_TVI_DS_InputSelected "(sélectionnée)"
#define MSGTR_TVI_DS_UnableExtractFreqTable "tvi_dshow: Impossible de lire la table des fréquence depuis kstvtune.ax\n"
#define MSGTR_TVI_DS_WrongDeviceParam "tvi_dshow: Mauvais paramêtre de périphérique : %s\n"
#define MSGTR_TVI_DS_WrongDeviceIndex "tvi_dshow: Mauvais index de périphérique: %d\n"
#define MSGTR_TVI_DS_WrongADeviceParam "tvi_dshow: Wrong adevice parameter: %s\n"
#define MSGTR_TVI_DS_WrongADeviceIndex "tvi_dshow: Wrong adevice index: %d\n"

#define MSGTR_TVI_DS_SamplerateNotsupported "tvi_dshow: Le taux d'échantillonage %d n'est pas supporté par le périphérique.  Retour au premier taux disponible.\n"
#define MSGTR_TVI_DS_VideoAdjustigNotSupported "tvi_dshow: Ajustement de la brillance/teinte/saturation/contraste non supportée par le périphérique\n"

#define MSGTR_TVI_DS_ChangingWidthHeightNotSupported "tvi_dshow: L'ajustement de la hauteur/largeur de la vidéo n'est pas supportée par le périphérique.\n"
#define MSGTR_TVI_DS_SelectingInputNotSupported  "tvi_dshow: La sélection de la source de capture n'est pas supportée par le périphérique\n"
#define MSGTR_TVI_DS_ErrorParsingAudioFormatStruct "tvi_dshow: Impossible d'analyser la structure du format audio.\n"
#define MSGTR_TVI_DS_ErrorParsingVideoFormatStruct "tvi_dshow: Impossible d'analyser la structure du format vidéo.\n"
#define MSGTR_TVI_DS_UnableSetAudioMode "tvi_dshow: Impossible d'utiliser le mode audio %d.  Erreur :0x%x\n"
#define MSGTR_TVI_DS_UnsupportedMediaType "tvi_dshow: Type de média non supporté passé vers %s\n"
#define MSGTR_TVI_DS_UnableFindNearestChannel "tvi_dshow: Impossible de trouver le canal le plus proche dans la table des fréquences du système\n"
#define MSGTR_TVI_DS_UnableToSetChannel "tvi_dshow: Impossible de basculer sur le canal le plus proche depuis la table des fréquences du système.  Erreur :0x%x\n"
#define MSGTR_TVI_DS_NoVideoCaptureDevice "tvi_dshow: Impossible de trouver un périphérique de capture vidéo\n"
#define MSGTR_TVI_DS_NoAudioCaptureDevice "tvi_dshow: Impossible de trouver un périphérique de capture audio\n"
#define MSGTR_TVI_DS_GetActualMediatypeFailed "tvi_dshow: Impossible d'obtenir le type de média réel (Erreur:0x%x).  Suppose qu'il s'agit de celui requis.\n"



/* Messages to be moved to the section where they belong in the English version */
