// Transated by: Firebird <firebird@chez.com>


// ========================= Aide MPlayer ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy (voir DOCS/AUTHORS)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Utilisation:   mplayer [-gui] [options] [répertoire/]fichier\n"
#else
"Utilisation:   mplayer [options] [répertoire/]fichier\n"
#endif
"\n"
"Options:\n"
" -vo <pil[:pér]> Sélectionne le pilote et le périphérique de sortie video (voir '-vo help' pour la liste)\n"
" -ao <pil[:pér]> Sélectionne le pilote et le périphérique de sortie audio (voir '-ao help' pour la liste)\n"
" -vcd <nrpiste>  Joue une piste VCD (Video CD) d'un périphérique plutôt que d'un fichier\n"
#ifdef HAVE_LIBCSS
" -dvdauth <pér>  Spécifie le périph. DVD pour l'auth. (pour disques encryptés)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <nrtitre>  Joue titre/piste DVD d'un périph. plutôt que d'un fichier\n"
#endif
" -ss <postemp>   Démarre la lecture à partir de la pos. (Secondes ou hh:mm:ss)\n"
" -nosound        Ne jouer aucun son\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  Séclect. la sortie stéréo MPEG1 (0:stereo 1:gauche 2:droite)\n"
#endif
" -fs -vm -zoom   Options pl. écr. (Plein écran, mode video, échel. par progr.)\n"
" -x <x> -y <y>   Échelonner l'image en <x> * <y> [si le pilote vo le supporte]\n"
" -sub <file>     Util. le fich. de sous-titres (voir aussi -subfps, -subdelay)\n"
" -vid x -aid y   Options pour jouer les courants video (x) et audio (y)\n"
" -fps x -srate y Options pour changer les fréq. video (x fps) et audio (y Hz)\n"
" -pp <qualité>   Activer le filtre de sortie (0-4 pour DivX, 0-63 pour MPEG)\n"
" -nobps          Utilise des méth. de sync A-V pour fichiers AVI (peut aider!)\n"
" -framedrop      Active le drop d'images (pour ordinateurs lents)\n"
"\n"
"Touches:\n"
" <- ou ->        Saute en avant ou en arrière de 10 secondes\n"
" haut / bas      Saute en avant ou en arrière de 1 minute\n"
" p ou ESPACE     pause (presser n'importe quelle touche pour continuer)\n"
" q ou ESC        Arrêter la lecture et quitter le programme\n"
" + ou -          Ajuster le délai audio de +/- 0.1 seconde\n"
" o               Mode OSD:  aucun / cherchable / cherchable+temps\n"
" * ou /          Augmenter/réduire vol. (presser 'm' pour sélect. maître/pcm)\n"
" z ou x          Ajuster le délai des sous-titres de +/- 0.1 Seconde\n"
"\n"
" * * * IL Y A D'AUTRES TOUCHES ET OPTIONS DANS LA PAGE MAN ! * * *\n"
"\n";
#endif

// ========================= Messages MPlayer ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nSortie... (%s)\n"
#define MSGTR_Exit_frames "Nombre demandé de frames jouées"
#define MSGTR_Exit_quit "Fin"
#define MSGTR_Exit_eof "Fin du fichier"
#define MSGTR_Exit_error "Erreur fatale"
#define MSGTR_IntBySignal "\nMPlayer interrompu par le signal %d du module: %s \n"
#define MSGTR_NoHomeDir "Ne peut trouver répertoire home\n"
#define MSGTR_GetpathProblem "Problème get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Création du fichier de config: %s\n"
#define MSGTR_InvalidVOdriver "Nom du pilote de sortie video invalide: %s\nUtiliser '-vo help' pour avoir une liste des pilotes disponibles.\n"
#define MSGTR_InvalidAOdriver "Nom du pilote de sortie audio invalide: %s\nUtiliser '-ao help' pour avoir une liste des pilotes disponibles.\n"
#define MSGTR_CopyCodecsConf "(Copie/lie etc/codecs.conf vers ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Ne peut charger la fonte: %s\n"
#define MSGTR_CantLoadSub "Ne peut charger les sous-titres: %s\n"
#define MSGTR_ErrorDVDkey "Erreur en processant la clé DVD.\n"
#define MSGTR_CmdlineDVDkey "La clé DVD requise à la ligne de commande a été sauvée pour décryptage.\n"
#define MSGTR_DVDauthOk "La séquence d'authentification DVD semble OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: le courant sélectionné est manquant\n"
#define MSGTR_CantOpenDumpfile "Ne peut ouvrir fichier dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS non spécifié (ou invalide) dans l'en-tête! Utiliser l'option -fps!\n"
#define MSGTR_NoVideoStream "Désolé, aucun courant video... c'est injouable\n"
#define MSGTR_TryForceAudioFmt "Tente de forcer famille de pilotes codec audio de famille %d ...\n"
#define MSGTR_CantFindAfmtFallback "Ne peut trouver de codec audio pour famille de pil. choisie, utilise d'autres.\n"
#define MSGTR_CantFindAudioCodec "Ne peut trouver codec pour format audio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Tente de mettre à jour %s de etc/codecs.conf\n*** Si ce n'est toujours pas bon, alors lisez DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Ne peut trouver de codec audio! -> Aucun son\n"
#define MSGTR_TryForceVideoFmt "Tente de forcer famille de pilotes codec video %d ...\n"
#define MSGTR_CantFindVfmtFallback "Ne peut trouver de codec video pour famille de pil. choisie, utilise d'autres.\n"
#define MSGTR_CantFindVideoCodec "Ne peut trouver codec pour format video 0x%X !\n"
#define MSGTR_VOincompCodec "Désolé, le pilote de sortie video choisi n'est pas compatible avec ce codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Ne peut initialiser le codec video :(\n"
#define MSGTR_EncodeFileExists "fichier déjà existant: %s (N'effacez pas vos AVIs préférés!)\n"
#define MSGTR_CantCreateEncodeFile "Ne peut ouvrir fichier pour encodage\n"
#define MSGTR_CannotInitVO "FATAL: Ne peut initialiser le pilote video!\n"
#define MSGTR_CannotInitAO "Ne peut ouvrir/initialiser le périphérique audio -> Aucun son\n"
#define MSGTR_StartPlaying "Démarre la reproduction...\n"
#define MSGTR_SystemTooSlow "\n************************************************************************"\
			    "\n** Votre système est trop lent. Essayez l'option -framedrop ou  RTFM! **"\
			    "\n************************************************************************\n"

#define MSGTR_NoGui "MPlayer fut compilé SANS support GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI nécessite X11!\n"
#define MSGTR_Playing "Joue %s\n"
#define MSGTR_NoSound "Audio: Aucun son!!!\n"
#define MSGTR_FPSforced "FPS fixé sur %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Lecteur CD-ROM '%s' non trouvé!\n"
#define MSGTR_ErrTrackSelect "Erreur lors du choix de la piste VCD!"
#define MSGTR_ReadSTDIN "Lit du stdin...\n"
#define MSGTR_UnableOpenURL "Ne sait ouvrir l'URL: %s\n"
#define MSGTR_ConnToServer "Connecté au serveur: %s\n"
#define MSGTR_FileNotFound "Fichier non trouvé: '%s'\n"

#define MSGTR_CantOpenDVD "Ne sait ouvrir le lecteur DVD: %s\n"
#define MSGTR_DVDwait "Lit la structure du disque, attendre svp...\n"
#define MSGTR_DVDnumTitles "Il y a %d titres sur ce DVD.\n"
#define MSGTR_DVDinvalidTitle "Numero de titre DVD invalide: %d\n"
#define MSGTR_DVDnumChapters "Il y a %d chapitres sur ce titre DVD.\n"
#define MSGTR_DVDinvalidChapter "Numéro de chapitre DVD invalide: %d\n"
#define MSGTR_DVDnumAngles "Il y a %d Séquences sur ce titre DVD.\n"
#define MSGTR_DVDinvalidAngle "Numéro de séquence DVD invalide: %d\n"
#define MSGTR_DVDnoIFO "Be peut ouvrir le fichier IDO pour le titre DVD %d.\n"
#define MSGTR_DVDnoVOBs "Ne peut ouvrir titre VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD ouvert avec succès!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Attention! En-tête du courant audio %d redéfini!\n"
#define MSGTR_VideoStreamRedefined "Attention! En-tête du courant video %d redéfini\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Trop (%d dans %d octets) de packets audio dans le tampon!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Trop (%d dans %d octets) de packets video dans le tampon!\n"
#define MSGTR_MaybeNI "Peut-être jouez-vous un courant/fichier non-interfolié ou le codec fait défaut..\n"
#define MSGTR_DetectedAVIfile "Format de fichier AVI détecté!\n"
#define MSGTR_DetectedASFfile "Format de fichier ASF détecté!\n"
#define MSGTR_DetectedMPEGPESfile "Format de fichier MPEG-PES détecté!\n"
#define MSGTR_DetectedMPEGPSfile "Format de fichier MPEG-PS détecté!\n"
#define MSGTR_DetectedMPEGESfile "Format de fichier MPEG-ES détecté!\n"
#define MSGTR_DetectedQTMOVfile "Format de fichier QuickTime/MOV détecté!\n"
#define MSGTR_MissingMpegVideo "Courant video MPEG manquant!? Contactez l'auteur, ceci pourrait être un bug :(\n"
#define MSGTR_InvalidMPEGES "Courant MPEG-ES invalide??? Contactez l'auteur, ceci pourrait être un bug :(\n"
#define MSGTR_FormatNotRecognized "========== Désolé, ce format de fichier n'est pas reconnu/supporté ===========\n"\
				  "======== Si ce fichier est un courant  AVI, ASF ou MPEG Stream sein, =========\n"\
				  "===================== alors veuillez contacter l'auteur ======================\n"
#define MSGTR_MissingVideoStream "Ne peut trouver de courant video!\n"
#define MSGTR_MissingAudioStream "Ne peut trouver de courant audio...  -> aucun son\n"
#define MSGTR_MissingVideoStreamBug "Courant video manquant!? Contactez l'auteur, ceci pourrait être un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "Demux: le fichier ne contient pas le courant audio ou video sélectionné\n"

#define MSGTR_NI_Forced "Forcé"
#define MSGTR_NI_Detected "Détecté"
#define MSGTR_NI_Message "%s format de fichier AVI NON-INTERFOLIÉ!\n"

#define MSGTR_UsingNINI "Utilise fichier de format AVI NON-INTERFOLIÉ défectueux!\n"
#define MSGTR_CouldntDetFNo "Ne peut déterminer le nombre de frames (pour recherche absolue)  \n"
#define MSGTR_CantSeekRawAVI "Ne peut chercher un courant .AVI basique! (index requis, essayez l'option -idx!)\n"
#define MSGTR_CantSeekFile "Ne peut chercher dans ce fichier!  \n"

#define MSGTR_EncryptedVOB "Fichier VOB encrypté (pas compilé avec le support libcss)! Lire DOCS/DVD\n"
#define MSGTR_EncryptedVOBauth "Courant encrypté mais l'autentification ne fut pas demandée par vous!!\n"

#define MSGTR_MOVcomprhdr "MOV: Les en-têtes compressées ne sont pas (encore) supportées!\n"
#define MSGTR_MOVvariableFourCC "MOV: Attention! Variable FOURCC détectée!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Attention! Trop de pistes!"
#define MSGTR_MOVnotyetsupp "\n******** Format Quicktime MOV n'est pas encore supporté!!!!!!! *********\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Ne peut ouvrir le Codec\n"
#define MSGTR_CantCloseCodec "Ne peut fermer le Codec\n"

#define MSGTR_MissingDLLcodec "ERREUR: Ne peut trouver le Codec DirectShow requis: %s\n"
#define MSGTR_ACMiniterror "Ne peut charger/initialiser le Codec AUDIO Win32/ACM (fichier DLL manquant?)\n"
#define MSGTR_MissingLAVCcodec "Ne peut trouver le Codec '%s' de libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer a été compilé SANS support DirectShow!\n"
#define MSGTR_NoWfvSupport "Support des Codecs Win32désactivée, ou non disponible sur plateformes non-x86!\n"
#define MSGTR_NoDivx4Support "MPlayer a été compilé SANS le support DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer a été compilé SANS le support ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Codecs Audio Win32/ACM désactivés ou non disponibles sur plateformes non-x86 -> force -nosound :(\n"
#define MSGTR_NoDShowAudio "MPlayer a été compilé sans support DirectShow -> force -nosound :(\n"
#define MSGTR_NoOggVorbis "Codec Audio OggVorbis désactivé -> force -nosound :(\n"

#define MSGTR_MpegPPhint "ATTENTION! Vous avez demandé un filtre de sortie pour une video MPEG 1/2,\n" \
			 "           mais avez compilé MPlayer sans support de filtre MPEG 1/2!\n" \
			 "           #define MPEG12_POSTPROC dans config.h et recompilez libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: Fin du fichier lors de la recherche d'en-tête de séquence\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Ne peut lire l'en-tête de séquence!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Ne peut lire l'extension d'en-tête de séquence!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Mauvais en-tête de séquence!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Mauvaise extension d'en-tête de séquence!\n"

#define MSGTR_ShMemAllocFail "Ne peut alloué de mémoire partagée\n"
#define MSGTR_CantAllocAudioBuf "Ne peut allouer de tampon de sortie audio\n"
#define MSGTR_NoMemForDecodedImage "pas assez de mémoire pour le tampon d'image décodée (%ld Octets)\n"

#define MSGTR_AC3notvalid "Courant AC3 non-valide.\n"
#define MSGTR_AC3only48k "Seuls les courants 48000 Hz sont supportés.\n"
#define MSGTR_UnknownAudio "Format audio inconnu ou manquant -> Aucun son\n"

// LIRC:
#define MSGTR_SettingUpLIRC "définition du support LIRC...\n"
#define MSGTR_LIRCdisabled "Vous ne serez pas capable d'utiliser votre télécommande\n"
#define MSGTR_LIRCopenfailed "Impossible d'ouvrir le support LIRC!\n"
#define MSGTR_LIRCsocketerr "Quelque chose est défectueux avec le socket LIRC: %s\n"
#define MSGTR_LIRCcfgerr "Impossible de lire le fichier de config LIRC %s !\n"


// ====================== messages/boutons GUI ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "À propos ..."
#define MSGTR_FileSelect "Choisir fichier ..."
#define MSGTR_MessageBox "BoiteMessage"
#define MSGTR_PlayList "PlayList"
#define MSGTR_SkinBrowser "Vision des peaux"

// --- boutons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Annuler"
#define MSGTR_Add "Ajouter"
#define MSGTR_Remove "Retirer"

// --- messages d'erreur ---
#define MSGTR_NEMDB "Désolé, pas assez de mémoire pour le tampon dessin."
#define MSGTR_NEMFMR "Désolé, pas assez de mémoire pour le rendu des menus."
#define MSGTR_NEMFMM "Désolé, pas assez de mémoire pour le masque de la fenêtre principale."

// --- messages d'erreurs du chargement de peau ---
#define MSGTR_SKIN_ERRORMESSAGE "[Peau] Erreur à la ligne %d du fichier de config de peau: %s" 
#define MSGTR_SKIN_WARNING1 "[Peau] Attention à la ligne %d du fichier de config de peau: Widget trouvé mais \"section\" ne fut pas trouvé avant (%s)"
#define MSGTR_SKIN_WARNING2 "[Peau] Attention à la ligne %d du fichier de config de peau: Widget trouvé mais \"subsection\" ne fut pas trouvé avant (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmaps de 16 bits ou moins ne sont pas supportés ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Fichier non trouvé ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "Erreur de lecture BMP ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "Erreur de lecture TGA ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "Erreur de lecture PNG ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "Tga empaquetés en RLE non supportés ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "format de fichier inconnu ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "erreur de conversion de 24 bit à 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "Message inconnue: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "pas assez de mémoire\n"

#define MSGTR_SKIN_FONT_TooManyFontsDeclared "trop de fontes déclarées\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "fichier de fonte introuvable\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "fichier d'image de fonte introuvable\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificateur de fonte inexistant ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "paramètre inconnu ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[Parcour de peau] pas assez de mémoire.\n"

#endif
