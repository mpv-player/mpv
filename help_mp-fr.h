// Original transation by Firebird <firebird@chez.com>
// Updates & fixes by pl <p_l@gmx.fr>

// ========================= Aide MPlayer ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (lisez les DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Utilisation:   mplayer [-gui] [options] [répertoire/]fichier\n"
#else
"Utilisation:   mplayer [options] [répertoire/]fichier\n"
#endif
"\n"
"Options de base (voir la page man pour TOUTES les autres options):\n"
" -vo <pil[:pér]>  Sél. le pil. et le périph. vidéo ('-vo help' pour la liste)\n"
" -ao <pil[:pér]>  Sél. le pil. et le périph. audio ('-ao help' pour la liste)\n"
#ifdef HAVE_VCD
" -vcd <n°piste>   Joue à partir d'un VCD plutôt que d'un fichier\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <pér>   Précise le chemin du lecteur DVD (pour les DVD cryptés)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <nrtitre>   Joue à partir du lecteur DVD plutôt que d'un fichier\n"
" -alang/-slang    Sélectionne la langue pour l'audio/les sous-titres (fr,en,...)\n"
#endif
" -ss <temps>      Démarre la lecture à 'temps' (temps en secondes ou hh:mm:ss)\n"
" -nosound         Ne joue aucun son\n"
" -fs -vm -zoom    Options plein-écran (fs: plein-écran, vm: changement de mode\n"
"                  vidéo, zoom: changement de taille software)\n"
" -x <x> -y <y>    Résolution de l'affichage (chgts. de mode vidéo ou zoom soft)\n"
" -sub <fich>      Spécifie les sous-titres à utiliser (cf. -subfps, -subdelay)\n"
" -playlist <fich> Spécifie la liste des fichiers à jouer\n"
" -vid x -aid y    Spécifie les flux vidéos (x) et audio (y) à jouer\n"
" -fps x -srate y  Options pour changer les fréq. vidéo (x fps) et audio (y Hz)\n"
" -pp <qualité>    Filtres de sorties (voir page man et les docs)\n"
" -framedrop       \"Drop\" d'images (pour les machines lentes)\n"
"\n"
"Fonctions au clavier: (voir la page man et regarder aussi dans input.conf)\n"
" <- ou ->         + / - 10 secondes\n"
" haut ou bas      + / - 1 minute\n"
" PgUp ou PgDown   + / - de 10 minutes\n"
" < ou >           Fichier suivant / précédent dans la playlist\n"
" p ou ESPACE      Pause (presser n'importe quelle touche pour continuer)\n"
" q ou ESC         Quitter\n"
" + ou -           Synchro audio / vidéo: +/- 0.1 seconde\n"
" o                Change l'OSD: rien / barre de recherche / barre rech. + temps\n"
" * ou /           Augmente/diminue le volume PCM\n"
" z ou x           Synchro des sous-titres: +/- 0.1 seconde\n"
" r ou t           Pos. des sous-titres: plus haut/plus bas (cf. -vop expand !)\n"
"\n"
" *** VOIR LA PAGE MAN POUR LES DETAILS ET LES AUTRES OPTIONS (AVANCEES) ***\n"
"\n";
#endif

// ========================= Messages MPlayer ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nSortie... (%s)\n"
#define MSGTR_Exit_frames "Nombre demandé de frames joué"
#define MSGTR_Exit_quit "Fin"
#define MSGTR_Exit_eof "Fin du fichier"
#define MSGTR_Exit_error "Erreur fatale"
#define MSGTR_IntBySignal "\nMPlayer interrompu par le signal %d dans le module: %s \n"
#define MSGTR_NoHomeDir "Ne peut trouver répertoire home\n"
#define MSGTR_GetpathProblem "Problème get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Création du fichier de config: %s\n"
#define MSGTR_InvalidVOdriver "Nom du pilote de sortie vidéo invalide: %s\nUtiliser '-vo help' pour avoir une liste des pilotes disponibles.\n"
#define MSGTR_InvalidAOdriver "Nom du pilote de sortie audio invalide: %s\nUtiliser '-ao help' pour avoir une liste des pilotes disponibles.\n"
#define MSGTR_CopyCodecsConf "(Copiez/liez etc/codecs.conf (dans le source de MPlayer) vers ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Ne peut charger la police: %s\n"
#define MSGTR_CantLoadSub "Ne peut charger les sous-titres: %s\n"
#define MSGTR_ErrorDVDkey "Erreur avec la clé du DVD.\n"
#define MSGTR_CmdlineDVDkey "La clé DVD demandée sur la ligne de commande a été sauvegardée pour le décryptage.\n"
#define MSGTR_DVDauthOk "La séquence d'authentification DVD semble OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: le flux sélectionné est manquant\n"
#define MSGTR_CantOpenDumpfile "Ne peut ouvrir un fichier dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS non spécifié (ou invalide) dans l'entête! Utiliser l'option -fps!\n"
#define MSGTR_NoVideoStream "Désolé, aucun flux vidéo... c'est injouable\n"
#define MSGTR_TryForceAudioFmt "Tente de forcer famille de pilotes codec audio de famille %d ...\n"
#define MSGTR_CantFindAfmtFallback "Ne peut trouver de codec audio pour famille de pilotes choisie, utilise d'autres.\n"
#define MSGTR_CantFindAudioCodec "Ne peut trouver codec pour format audio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Tentez de mettre à jour %s à partir de etc/codecs.conf\n*** Si ce n'est toujours pas bon, alors lisez DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Ne peut trouver de codec audio! -> Aucun son\n"
#define MSGTR_TryForceVideoFmt "Tente de forcer famille de pilotes codec vidéo %d ...\n"
#define MSGTR_CantFindVfmtFallback "Ne peut trouver de codec vidéo pour famille de pil. choisie, utilise d'autres.\n"
#define MSGTR_CantFindVideoCodec "Ne peut trouver codec pour format vidéo 0x%X !\n"
#define MSGTR_VOincompCodec "Désolé, le pilote de sortie vidéo choisi n'est pas compatible avec ce codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Ne peut initialiser le codec vidéo :(\n"
#define MSGTR_EncodeFileExists "fichier déjà existant: %s (N'effacez pas vos AVIs préférés!)\n"
#define MSGTR_CantCreateEncodeFile "Ne peut ouvrir fichier pour encodage\n"
#define MSGTR_CannotInitVO "FATAL: Ne peut initialiser le pilote vidéo!\n"
#define MSGTR_CannotInitAO "Ne peut ouvrir/initialiser le périphérique audio -> Aucun son\n"
#define MSGTR_StartPlaying "Démarre la reproduction...\n"
#define MSGTR_SystemTooSlow "\n***********************************************************************"\
			    "\n** Votre système est trop lent. Essayez l'option -framedrop ou RTFM! **"\
			    "\n***********************************************************************\n"\
			    "!!! Raisons possibles, problèmes, solutions: \n"\
			    "- Le plus probable: pilote audio _buggé_ => essayer -ao sdl ou\n"\
			    "  ALSA 0.5 ou l'émulation OSS d'ALSA 0.9 => lire DOCS/sound.html\n"\
			    "- Vidéo lente => essayer avec plusieurs pilotes -vo (pour la liste: -vo help) ou\n"\
			    "  avec -framedrop => lire DOCS/video.html\n"\
			    "- CPU lent => éviter les gros DVD/DivX => essayer -hardframedrop\n"\
			    "- Fichier corrompu => essayer des mélanges de -nobps -ni -mc 0 -forceidx\n"\
			    "- -cache est utilisé avec un fichier mal multiplexé => essayer avec -nocache\n"\
			    "Si rien de tout cela ne résout le problème, lisez DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer a été compilé SANS support GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI a besoin de X11!\n"
#define MSGTR_Playing "Joue %s\n"
#define MSGTR_NoSound "Audio: Aucun son!!!\n"
#define MSGTR_FPSforced "FPS forcé à %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Lecteur CD-ROM '%s' non trouvé!\n"
#define MSGTR_ErrTrackSelect "Erreur lors du choix de la piste VCD!"
#define MSGTR_ReadSTDIN "Lecture depuis stdin...\n"
#define MSGTR_UnableOpenURL "Ne peut ouvrir l'URL: %s\n"
#define MSGTR_ConnToServer "Connecté au serveur: %s\n"
#define MSGTR_FileNotFound "Fichier non trouvé: '%s'\n"

#define MSGTR_CantOpenDVD "Ne peut ouvrir le lecteur DVD: %s\n"
#define MSGTR_DVDwait "Lecture de la structure du disque, veuillez attendre...\n"
#define MSGTR_DVDnumTitles "Il y a %d titres sur ce DVD.\n"
#define MSGTR_DVDinvalidTitle "Numéro de titre DVD invalide: %d\n"
#define MSGTR_DVDnumChapters "Il y a %d chapitres sur ce titre DVD.\n"
#define MSGTR_DVDinvalidChapter "Numéro de chapitre DVD invalide: %d\n"
#define MSGTR_DVDnumAngles "Il y a %d séquences sur ce titre DVD.\n"
#define MSGTR_DVDinvalidAngle "Numéro de séquence DVD invalide: %d\n"
#define MSGTR_DVDnoIFO "Ne peut ouvrir le fichier IFO pour le titre DVD %d.\n"
#define MSGTR_DVDnoVOBs "Ne peut ouvrir titre VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD ouvert avec succès!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Attention! Entête du flux audio %d redéfini!\n"
#define MSGTR_VideoStreamRedefined "Attention! Entête du flux vidéo %d redéfini!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Trop (%d dans %d octets) de packets audio dans le tampon!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Trop (%d dans %d octets) de packets vidéo dans le tampon!\n"
#define MSGTR_MaybeNI "(Peut-être jouez-vous un flux/fichier mal multiplexé, ou le codec manque...)\n"\
                      "Pour les fichier .AVI, essayez l'option -ni."
#define MSGTR_DetectedFILMfile "Format de fichier FILM détecté!\n"
#define MSGTR_DetectedFLIfile "Format de fichier FLI détecté!\n"
#define MSGTR_DetectedROQfile "Format de fichier RoQ détecté!\n"
#define MSGTR_DetectedREALfile "Format de fichier REAL détecté!\n"
#define MSGTR_DetectedAVIfile "Format de fichier AVI détecté!\n"
#define MSGTR_DetectedASFfile "Format de fichier ASF détecté!\n"
#define MSGTR_DetectedMPEGPESfile "Format de fichier MPEG-PES détecté!\n"
#define MSGTR_DetectedMPEGPSfile "Format de fichier MPEG-PS détecté!\n"
#define MSGTR_DetectedMPEGESfile "Format de fichier MPEG-ES détecté!\n"
#define MSGTR_DetectedQTMOVfile "Format de fichier QuickTime/MOV détecté!\n"
#define MSGTR_MissingMpegVideo "Flux vidéo MPEG manquant!? Contactez l'auteur, c'est peut-être un bug :(\n"
#define MSGTR_InvalidMPEGES "Flux MPEG-ES invalide??? Contactez l'auteur, c'est peut-être un bug :(\n"
#define MSGTR_FormatNotRecognized "========== Désolé, ce format de fichier n'est pas reconnu/supporté ===========\n"\
				  "========= Si ce fichier est un fichier AVI, ASF ou MPEG en bon état, =========\n"\
				  "======================= merci de contacter l'auteur ! ========================\n"
#define MSGTR_MissingVideoStream "Ne peut trouver de flux vidéo!\n"
#define MSGTR_MissingAudioStream "Ne peut trouver de flux audio...  -> pas de son\n"
#define MSGTR_MissingVideoStreamBug "Flux vidéo manquant!? Contactez l'auteur, c'est peut-être un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "Demux: le fichier ne contient pas le flux audio ou vidéo sélectionné\n"

#define MSGTR_NI_Forced "Forcé"
#define MSGTR_NI_Detected "Détecté"
#define MSGTR_NI_Message "%s format de fichier AVI mal multiplexé!\n"

#define MSGTR_UsingNINI "Utilise le support des fichiers AVI mal multiplexés!\n"
#define MSGTR_CouldntDetFNo "Ne peut déterminer le nombre de frames (pour recherche absolue)\n"
#define MSGTR_CantSeekRawAVI "Ne peut chercher dans un flux .AVI brut! (index requis, essayez l'option -idx!)\n"
#define MSGTR_CantSeekFile "Ne peut chercher dans ce fichier!  \n"

#define MSGTR_EncryptedVOB "Fichier VOB crypté (support libcss NON compilé!) Lire DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Flux crypté mais l'authentification n'a pas été demandée explicitement!\n"

#define MSGTR_MOVcomprhdr "MOV: Les entêtes compressées ne sont pas (encore) supportés!\n"
#define MSGTR_MOVvariableFourCC "MOV: Attention! Variable FOURCC détectée!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Attention! Trop de pistes!"
#define MSGTR_MOVnotyetsupp "\n******** Format Quicktime MOV pas encore supporté!!!!!!! *********\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Ne peut ouvrir le codec\n"
#define MSGTR_CantCloseCodec "Ne peut fermer le codec\n"

#define MSGTR_MissingDLLcodec "ERREUR: Ne peut trouver le codec DirectShow requis: %s\n"
#define MSGTR_ACMiniterror "Ne peut charger/initialiser le codec AUDIO Win32/ACM (fichier DLL manquant?)\n"
#define MSGTR_MissingLAVCcodec "Ne peut trouver le codec '%s' de libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer a été compilé SANS support DirectShow!\n"
#define MSGTR_NoWfvSupport "Support des codecs Win32 désactivé, ou non disponible sur plateformes non-x86!\n"
#define MSGTR_NoDivx4Support "MPlayer a été compilé SANS le support DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer a été compilé SANS le support ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Codecs audio Win32/ACM désactivés ou non disponibles sur plateformes non-x86 -> force -nosound :(\n"
#define MSGTR_NoDShowAudio "MPlayer a été compilé sans support DirectShow -> force -nosound :(\n"
#define MSGTR_NoOggVorbis "Codec audio OggVorbis désactivé -> force -nosound :(\n"
#define MSGTR_NoXAnimSupport "MPlayer a été compilé SANS support XAnim!\n"

#define MSGTR_MpegPPhint "ATTENTION! Vous avez demandé un filtre de sortie pour une vidéo MPEG 1/2,\n" \
			 "           mais avez compilé MPlayer sans support de filtre MPEG 1/2!\n" \
			 "           #define MPEG12_POSTPROC dans config.h et recompilez libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: Fin du fichier lors de la recherche d'entête de séquence\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Ne peut lire l'entête de séquence!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Ne peut lire l'extension d'entête de séquence!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Mauvais entête de séquence!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Mauvaise extension d'entête de séquence!\n"

#define MSGTR_ShMemAllocFail "Ne peut allouer de mémoire partagée\n"
#define MSGTR_CantAllocAudioBuf "Ne peut allouer de tampon de sortie audio\n"
#define MSGTR_NoMemForDecodedImage "pas assez de mémoire pour le tampon d'image décodée (%ld octets)\n"

#define MSGTR_AC3notvalid "Flux AC3 non-valide.\n"
#define MSGTR_AC3only48k "Seuls les flux à 48000 Hz sont supportés.\n"
#define MSGTR_UnknownAudio "Format audio inconnu/manquant -> pas de son\n"

// LIRC:
#define MSGTR_SettingUpLIRC "définition du support LIRC...\n"
#define MSGTR_LIRCdisabled "Vous ne pourrez pas utiliser votre télécommande\n"
#define MSGTR_LIRCopenfailed "Impossible d'ouvrir le support LIRC!\n"
#define MSGTR_LIRCsocketerr "Il y a un problème avec le socket LIRC: %s\n"
#define MSGTR_LIRCcfgerr "Impossible de lire le fichier de config LIRC %s !\n"


// ====================== messages/boutons GUI ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "À propos ..."
#define MSGTR_FileSelect "Choisir un fichier ..."
#define MSGTR_SubtitleSelect "Choisir un sous-titre ..."
#define MSGTR_OtherSelect "Choisir ..."
#define MSGTR_MessageBox "BoiteMessage"
#define MSGTR_PlayList "Playlist"
#define MSGTR_SkinBrowser "Browser de skins"

// --- boutons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Annuler"
#define MSGTR_Add "Ajouter"
#define MSGTR_Remove "Retirer"

// --- messages d'erreur ---
#define MSGTR_NEMDB "Désolé, pas assez de mémoire pour le tampon de dessin."
#define MSGTR_NEMFMR "Désolé, pas assez de mémoire pour le rendu des menus."
#define MSGTR_NEMFMM "Désolé, pas assez de mémoire pour le masque de la fenêtre principale."

// --- messages d'erreurs du chargement de skin ---
#define MSGTR_SKIN_ERRORMESSAGE "[Skin] Erreur à la ligne %d du fichier de config de skin: %s" 
#define MSGTR_SKIN_WARNING1 "[Skin] Attention à la ligne %d du fichier de config de skin: Widget trouvé mais \"section\" n'a pas été trouvé avant (%s)"
#define MSGTR_SKIN_WARNING2 "[Skin] Attention à la ligne %d du fichier de config de skin: Widget trouvé mais \"subsection\" n'a pas été trouvé avant (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmaps de 16 bits ou moins ne sont pas supportés ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Fichier non trouvé ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "Erreur de lecture BMP ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "Erreur de lecture TGA ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "Erreur de lecture PNG ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "tga compacté en RLE non supportés ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "format de fichier inconnu ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "erreur de conversion de 24 bit à 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "Message inconnu: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "pas assez de mémoire\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "trop de polices déclarées\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "fichier de police introuvable\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "fichier d'image de police introuvable\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificateur de fonte inexistant ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "paramètre inconnu ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[Browser de skins] pas assez de mémoire.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin non trouvé ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Erreur de lecture du fichier de configuration du skin ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- menus gtk
#define MSGTR_MENU_AboutMPlayer "À propos de MPlayer"
#define MSGTR_MENU_Open "Ouvrir ..."
#define MSGTR_MENU_PlayFile "Lire un fichier ..."
#define MSGTR_MENU_PlayVCD "Lire un VCD ..."
#define MSGTR_MENU_PlayDVD "Lire un DVD ..."
#define MSGTR_MENU_PlayURL "Lire une URL ..."
#define MSGTR_MENU_LoadSubtitle "Charger un sous-titre ..."
#define MSGTR_MENU_Playing "En cours de lecture"
#define MSGTR_MENU_Play "Lecture"
#define MSGTR_MENU_Pause "Pause"
#define MSGTR_MENU_Stop "Arrêt"
#define MSGTR_MENU_NextStream "Flux suivant"
#define MSGTR_MENU_PrevStream "Flux précédent"
#define MSGTR_MENU_Size "Taille"
#define MSGTR_MENU_NormalSize "Taille normale"
#define MSGTR_MENU_DoubleSize "Taille double"
#define MSGTR_MENU_FullScreen "Plein écran"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Lire un disque..."
#define MSGTR_MENU_ShowDVDMenu "Afficher le menu DVD"
#define MSGTR_MENU_Titles "Titres"
#define MSGTR_MENU_Title "Titre %2d"
#define MSGTR_MENU_None "(aucun)"
#define MSGTR_MENU_Chapters "Chapitres"
#define MSGTR_MENU_Chapter "Chapitre %2d"
#define MSGTR_MENU_AudioLanguages "Langues (audio)"
#define MSGTR_MENU_SubtitleLanguages "Langues (sous-titres)"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Browser de skins"
#define MSGTR_MENU_Preferences "Préférences"
#define MSGTR_MENU_Exit "Quitter ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Erreur fatale ..."
#define MSGTR_MSGBOX_LABEL_Error "erreur ..."
#define MSGTR_MSGBOX_LABEL_Warning "attention ..." 

#endif
