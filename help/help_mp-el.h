// Translated by: Ioannis Panteleakis <pioann@csd.auth.gr>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (βλέπε DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Usage:   mplayer [-gui] [επιλογές] [διαδρομή/]όνομα_αρχείου\n"
#else
"Usage:   mplayer [επιλογές] [διαδρομή/]όνομα_αρχείου\n"
#endif
"\n"
"Επιλογές:\n"
" -vo <drv[:dev]> επιλέξτε τον οδηγό εξόδου βίντεο και τη συσκευή (βλέπε '-vo help' για τη λίστα)\n"
" -ao <drv[:dev]> επιλέξτε τον οδηγό εξόδου ήχου και τη συσκευή (βλέπε '-ao help' για τη λίστα)\n"
" -vcd <trackno>  αναπαραγωγή VCD (video cd) track από συσκευή αντί για αρχείο\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  ορίζει τη συσκευή DVD για πιστοποίηση (για κρυπτογραφημένους δίσκους)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  αναπαραγωγή του τίτλου/track DVD από τη συσκευή αντί για αρχείο\n"
#endif
" -ss <timepos>   αναζήτηση σε δεδομένη θέση (δευτερόλεπτα ή hh:mm:ss)\n"
" -nosound        μη αναπαραγωγή του ήχου\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  επιλογή εξόδου MPEG1 stereo (0:stereo 1:αριστερά 2:δεξιά)\n"
#endif
" -channels <n>   ο αριθμός των καναλιών εξόδου του ήχου\n"
" -fs -vm -zoom   επιλογές για αναπαραγωγή σε πλήρη οθόνη (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   κλημάκωση εικόνας σε <x> * <y> αναλύσεις [αν ο -vo οδηγός το υποστηρίζει!]\n"
" -sub <αρχείο>   επιλογή του αρχείου υποτίτλων για χρήση (βλέπε επίσης -subfps, -subdelay)\n"
" -playlist <αρχείο> ορίζει το αρχείο της playlist\n"
" -vid x -aid y   επιλογές για επιλογή βίντεο (x) και ήχο (y) stream για αναπαραγωγή\n"
" -fps x -srate y επιλογές για την αλλαγή της συχνότητας του βίντεο (x fps) και του ήχου (y Hz)\n"
" -pp <ποιότητα>  ενεργοποίηση του φίλτρου postprocessing (0-4 για DivX, 0-63 για mpegs)\n"
" -nobps          χρήση εναλλακτικής μεθόδου συγχρονισμού A-V για AVI αρχεία (μπορεί να βοηθήσει!)\n"
" -framedrop      ενεργοποίηση του frame-dropping (για αργά μηχανήματα)\n"
" -wid <id παραθύρου> χρήση τρέχον παραθύρου για έξοδο βίντεο (χρήσιμο με plugger!)\n"
"\n"
"Keys:\n"
" <-  or  ->      αναζήτηση μπρος/πίσω κατά 10 δευτερόλεπτα\n"
" up or down      αναζήτηση μπρος/πίσω κατά 1 λεπτό\n"
" < or >          αναζήτηση μπρος/πίσω στην playlist\n"
" p or SPACE      παύση ταινίας (πατήστε οποιοδήποτε πλήκτρο για να συνεχίσετε)\n"
" q or ESC        στοπ την αναπαραγωγή και έξοδος προγράμματος\n"
" + or -          ρύθμιση καθυστέρισης ήχου κατά +/- 0.1 δευτερόλεπτα\n"
" o               αλλαγή της OSD μεθόδου:  τίποτα / seekbar / seekbar+χρόνος\n"
" * or /          αύξηση ή μείωση της έντασης του ήχου (πατήστε 'm' για επιλογή master/pcm)\n"
" z or x          ρύθμιση καθυστέρισης υποτίτλων κατά +/- 0.1 δευτερόλεπτα\n"
"\n"
" * * * ΒΛΕΠΕ MANPAGE ΓΙΑ ΠΕΡΙΣΣΟΤΕΡΕΣ ΛΕΠΤΟΜΕΡΕΙΕΣ, ΚΑΙ ΠΙΟ ΠΡΟΧΩΡΙΜΕΝΕΣ ΕΠΙΛΟΓΕΣ ΚΑΙ KEYS ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nΈξοδος... (%s)\n"
#define MSGTR_Exit_frames "Ο επιλεγμένος αριθμός των frames έχουν αναπαραγωγεί"
#define MSGTR_Exit_quit "Κλείσιμο"
#define MSGTR_Exit_eof "Τέλος του αρχείου"
#define MSGTR_Exit_error "Κρίσιμο σφάλμα"
#define MSGTR_IntBySignal "\nΤο MPlayer τερματήστηκε από το σήμα %d στο module: %s \n"
#define MSGTR_NoHomeDir "Μη δυνατή η εύρεση του HOME φακέλου\n"
#define MSGTR_GetpathProblem "get_path(\"config\") πρόβλμηα\n"
#define MSGTR_CreatingCfgFile "Δημιουργία του αρχείου config: %s\n"
#define MSGTR_InvalidVOdriver "Λάθος όνομα για τον οδηγό εξόδου βίντεο: %s\nΧρησιμοποιήστε '-vo help' για να έχετε τη λίστα των διαθέσιμων οδηγών εξόδου βίντεο.\n"
#define MSGTR_InvalidAOdriver "Λάθος όνομα για τον οδηγό εξόδου ήχου: %s\nΧρησιμοποιήστε '-ao help' για να έχετε τη λίστα των διαθέσιμων οδηγών εξόδου ήχου.\n"
#define MSGTR_CopyCodecsConf "(αντιγραφή/ln etc/codecs.conf (από τον πηγαίο του MPlayer) στο ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Μη δυνατότητα φώρτωσης της γραμματοσειράς: %s\n"
#define MSGTR_CantLoadSub "Μη δυνατότητα φώρτωσης των υποτίτλων: %s\n"
#define MSGTR_ErrorDVDkey "Σφάλμα κατά την επεξεργασία του DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "Το ζητούμενο κλειδί για το DVD αποθυκεύτηκε για descrambling.\n"
#define MSGTR_DVDauthOk "Η ακολουθία πιστοποίησης του DVD φαίνεται εντάξει.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: ΚΡΙΣΙΜΟ: λοίπει το επιλεγμένο stream!\n"
#define MSGTR_CantOpenDumpfile "Αδύνατο το άνοιγμα του dump αρχείου!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Μη ορισμένα FPS (ή λάθος) στο header! Χρησιμοποιήστε την επιλογή -fps!\n"
#define MSGTR_NoVideoStream "Λυπάμαι, δεν υπάρχει βίντεο stream... δεν μπορεί να αναπαραγωγεί ακόμα\n"
#define MSGTR_TryForceAudioFmt "Προσπάθεια να επιβολής της οικογένειας του οδηγού του codec του ήχου %d ...\n"
#define MSGTR_CantFindAfmtFallback "Δεν είναι δυνατή η εύρεση της οικογένειας του οδηγού του codec του ήχου, χρήση άλλου οδηγού.\n"
#define MSGTR_CantFindAudioCodec "Δεν είναι δυνατή η εύρεση του format του codec του ήχου 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Δοκιμάστε να αναβαθμίσεται το %s από το etc/codecs.conf\n*** Αν ακόμα υπάρχει πρόβήμα, διαβάστε DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Αδύνατη η αρχικοποίηση του codec του ήχου! -> χωρίς-ήχο\n"
#define MSGTR_TryForceVideoFmt "Προσπάθεια να επιβολής της οικογένειας του οδηγού του codec του βίντεο %d ...\n"
#define MSGTR_CantFindVfmtFallback "Δεν είναι δυνατή η εύρεση της οικογένειας του οδηγού του codec του βίντεο, χρήση άλλου οδηγού.\n"
#define MSGTR_CantFindVideoCodec "Δεν είναι δυνατή η εύρεση του codec για τον συγκεκριμένο -vo και το format του βίντεο 0x%X !\n"
#define MSGTR_VOincompCodec "Λυπάμαι, η επιλεγμένη συσκευή video_out είναι ασύμβατη με αθτό το codec.\n"
#define MSGTR_CouldntInitVideoCodec "ΚΡΙΣΙΜΟ: Αδύνατη η αρχικοποίηση του codec του βίντεο :(\n"
#define MSGTR_EncodeFileExists "Το αρχείο υπάρχει ήδη: %s (μην διαγράψετε το αγαπημένο σας AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Αδύνατη η δημιουργία του αρχείου για κωδικοποίηση\n"
#define MSGTR_CannotInitVO "ΚΡΙΣΙΜΟ: Αδύνατη η αρχικοποίηση του οδηγού του βίντεο!\n"
#define MSGTR_CannotInitAO "αδύνατο το άνοιγμα/αρχικοποίηση του οδηγού του ήχου -> ΧΩΡΙΣ-ΗΧΟ\n"
#define MSGTR_StartPlaying "Εκκίνιση αναπαραγωγής...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         **************************************************************************\n"\
"         **** Το σύστημά σας είναι πολύ αργό για την αναπαραγωγή του αρχείου!  ****\n"\
"         **************************************************************************\n"\
"!!! Πιθανές αιτίες, προβλήματα, λύσεις: \n"\
"- Συνήθη αιτία: πρόβλημα με τον οδηγό του ήχου. λύση: δοκιμάστε -ao sdl ή χρησιμοποιήστε\n"\
"  ALSA 0.5 ή oss emulation του οδηγού ALSA 0.9. Διαβάστε DOCS/sound.html για περισσότερες λύσεις!\n"\
"- Αργή έξοδος του βίντεο. Δοκιμάστε διαφορετικό -vo οδηγό (για λίστα: -vo help) ή δοκιμάστε\n"\
"  με -framedrop !  Διαβάστε DOCS/video.html για ρύθμιση/επιτάχυνση του βίντεο.\n"\
"- Αργός επεξεργαστής. Μην αναπαράγετε μεγάλα dvd/divx σε αργούς επεξεργαστές! δοοκιμάστε με -hardframedrop\n"\
"- Broken file. δοκιμάστε με διάφορους συνδιασμούς από τα παρακάτω: -nobps  -ni  -mc 0  -forceidx\n"\
"Αν κανένα από αυτά δεν κάνει, τότε διαβάστε DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "Το MPlayer μεταφράστηκε ΧΩΡΙΣ υποστήριξη για GUI!\n"
#define MSGTR_GuiNeedsX "Το GUI του MPlayer χρειάζεται X11!\n"
#define MSGTR_Playing "Αναπαραγωγή του %s\n"
#define MSGTR_NoSound "Ήχος: μη διαθέσιμο!!!\n"
#define MSGTR_FPSforced "Τα FPS ρυθμίστηκαν να είναι %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Η CD-ROM συσκευή '%s' δεν βρέθηκε!\n"
#define MSGTR_ErrTrackSelect "Σφάλμα στην επιλογή του VCD track!"
#define MSGTR_ReadSTDIN "Διαβάζοντας από το stdin...\n"
#define MSGTR_UnableOpenURL "Αδύνατο το άνοιγμα του URL: %s\n"
#define MSGTR_ConnToServer "Πραγματοποιήθηκε σύνδεση με τον server: %s\n"
#define MSGTR_FileNotFound "Το αρχείο: '%s' δεν βρέθηκε\n"

#define MSGTR_CantOpenDVD "Δεν μπόρεσα να ανοίξω την DVD συσκευή: %s\n"
#define MSGTR_DVDwait "Ανάγνωση δομής του δίσκου, παρακαλώ περιμένετε...\n"
#define MSGTR_DVDnumTitles "Υπάρχουν %d τίτλοι στο DVD.\n"
#define MSGTR_DVDinvalidTitle "Invalid DVD title number: %d\n"
#define MSGTR_DVDnumChapters "Υπάρχουν %d κεφάλαια σε αυτόν τον τίτλο του DVD.\n"
#define MSGTR_DVDinvalidChapter "Λάθος αριθμός των κεφαλαίων του DVD: %d\n"
#define MSGTR_DVDnumAngles "Υπάρχουν %d γωνίεςσε αυτό τον τίτλο του DVD.\n"
#define MSGTR_DVDinvalidAngle "Λάθος αριθμός των γωνιών του DVD: %d\n"
#define MSGTR_DVDnoIFO "Δεν είναι δυνατό το άνοιγμα του IFO αρχείο για τον τίτλο του DVD %d.\n"
#define MSGTR_DVDnoVOBs "Δεν είναι δυνατό το άνοιγμα των VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "Το DVD άνοιξε με επιτυχία!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Προειδοποίηση! Η επικεφαλίδα του καναλιού ήχου %d ορίζεται ξανά!\n"
#define MSGTR_VideoStreamRedefined "Προειδοποίηση! Η επικεφαλίδα του καναλιού βίντεο %d ορίζεται ξανά!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Πολλαπλά (%d σε %d bytes) πακέτα ήχου στον buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Πολλαπλά (%d σε %d bytes) πακέτα βίντεο στον buffer!\n"
#define MSGTR_MaybeNI "(ίσως αναπαράγεται έναν non-interleaved κανάλι/αρχείο ή απέτυχε το codec)\n"
#define MSGTR_DetectedFILMfile "Αναγνωρίστηκε αρχείο τύπου FILM!\n"
#define MSGTR_DetectedFLIfile "Αναγνωρίστηκε αρχείο τύπου FLI!\n"
#define MSGTR_DetectedROQfile "Αναγνωρίστηκε αρχείο τύπου RoQ!\n"
#define MSGTR_DetectedREALfile "Αναγνωρίστηκε αρχείο τύπου REAL!\n"
#define MSGTR_DetectedAVIfile "Αναγνωρίστηκε αρχείο τύπου AVI!\n"
#define MSGTR_DetectedASFfile "Αναγνωρίστηκε αρχείο τύπου ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Αναγνωρίστηκε αρχείο τύπου MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Αναγνωρίστηκε αρχείο τύπου MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Αναγνωρίστηκε αρχείο τύπου MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Αναγνωρίστηκε αρχείο τύπου QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Λείπει το κανάλι βίντεο MPEG!? Επικοινώνησε με τον author, μπορεί να είναι ένα bug :(\n"
#define MSGTR_InvalidMPEGES "Μη Αναγνωρίσιμο κανάλι MPEG-ES??? Επικοινώνησε με τον author, μπορεί να είναι ένα bug :(\n"
#define MSGTR_FormatNotRecognized "============= Λυπάμαι, αυτό το είδος αρχείου δεν αναγνωρίζεται/υποστηρίζεται ===============\n"\
				  "=== If this file is an AVI, ASF or MPEG stream, παρακαλώ επικοινωνήστε με τον author! ===\n"
#define MSGTR_MissingVideoStream "Δεν βρέθηκε κανάλι βίντεο!\n"
#define MSGTR_MissingAudioStream "Δεν βρέθηκε κανάλι ήχου...  ->χωρίς-ήχο\n"
#define MSGTR_MissingVideoStreamBug "Λείπει το κανάλι βίντεο!? Επικοινώνησε με τον author, μπορεί να είναι ένα bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: το αρχείο δεν περιέχει το επιλεγμένο κανάλι ήχου ή βίντεο\n"

#define MSGTR_NI_Forced "Forced"
#define MSGTR_NI_Detected "Βρέθηκε"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI format αρχείου!\n"

#define MSGTR_UsingNINI "Χρήση ενός NON-INTERLEAVED φθαρμένου αρχείου τύπου AVI!\n"
#define MSGTR_CouldntDetFNo "Δεν μπόρεσε να διεκρυνιστεί ο αριθμός των frames (για απόλυτη αναζήτηση)  \n"
#define MSGTR_CantSeekRawAVI "Μη δυνατη αναζήτηση σε raw .AVI streams! (το index είναι απαραίτητο, δοκιμάστε με την επιλογή -idx!)  \n"
#define MSGTR_CantSeekFile "Αδύνατη η αναζήτηση σε αυτό το αρχείο!  \n"

#define MSGTR_EncryptedVOB "Κωδικοποιημένο VOB αρχείο (η μετάφραση έγινε χωρίς την libcss υποστήριξη)! Διαβάστε DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Κωδικοποιημένο stream αλλά δεν ζητήθηκε πιστοποίηση!!\n"

#define MSGTR_MOVcomprhdr "MOV: Συμπιεσμένα headers δεν υποστηρίζονται (ακόμα)!\n"
#define MSGTR_MOVvariableFourCC "MOV: ΠΡΟΕΙΔΟΠΟΙΗΣΗ! μεταβλητό FOURCC βρέθηκε!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Προειδοποίηση! βρέθηκαν πολλά tracks!"
#define MSGTR_MOVnotyetsupp "\n****** Το Quicktime MOV format δεν υποστηρίζεται ακόμα!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Αδύνατο το άνοιγμα του codec\n"
#define MSGTR_CantCloseCodec "Αδύνατο το κλείσιμο του codec\n"

#define MSGTR_MissingDLLcodec "ΣΦΑΛΜΑ: Δεν είναι δυνατό το άνοιγμα του απαιτούμενο DirectShow codec: %s\n"
#define MSGTR_ACMiniterror "Δεν είναι δυνατό να φορτωθεί/αρχικοποιηθεί το Win32/ACM codec ήχου (λείπει το DLL αρχείο?)\n"
#define MSGTR_MissingLAVCcodec "Δεν είανι δυνατό να βρεθεί το '%s' στο libavcodec...\n"

#define MSGTR_NoDShowSupport "Το MPlayer μεταγλωττήστηκε ΧΩΡΙΣ υποστήριξη για directshow!\n"
#define MSGTR_NoWfvSupport "Απενεργοποιημένη η υποστήριξη για τα win32 codecs, ή μη διαθέσιμα για μη-x86 πλατφόρμες!\n"
#define MSGTR_NoDivx4Support "Το MPlayer μεταγλωττήστηκε ΧΩΡΙΣ υποστήριξη για DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "Το MPlayer μεταγλωττήστηκε ΧΩΡΙΣ υποστήριξη για ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Απενεργοποιημένη η υποστήριξη για Win32/ACM codec ήχου, ή μη διαθέσιμα για μη-x86 πλατφόρμες -> χωρίς-ήχο :(\n"
#define MSGTR_NoDShowAudio "Το MPlayer μεταγλωττήστηκε ΧΩΡΙΣ υποστήριξη για DirectShow -> χωρίς-ήχο :(\n"
#define MSGTR_NoOggVorbis "Το OggVorbis codec ήχου είναι απενεργοποιημένο -> χωρίς-ήχο :(\n"
#define MSGTR_NoXAnimSupport "Το MPlayer μεταγλωττήστηκε ΧΩΡΙΣ υποστήριξη για XAnim!\n"

#define MSGTR_MpegPPhint "ΠΡΟΕΙΔΟΠΟΙΗΣΗ! Ζητήθηκε postprocessing εικόνας για MPEG 1/2 βίντεο,\n" \
			 "         αλλά το MPlayer μεταγλωττήστηκε ΧΩΡΙΣ υποστήριξη για MPEG 1/2 postprocessing!\n" \
			 "         αλλάξτε τη γραμμή #define MPEG12_POSTPROC στο config.h, και ξαναμεταγλωττήστε το libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: ΚΡΙΣΙΜΟ: βρέθηκε EOF στην αναζήτηση για ακολουθία της επικεφαλίδας\n"
#define MSGTR_CannotReadMpegSequHdr "ΚΡΙΣΙΜΟ: Δεν είναι δυνατό να διαβαστεί η ακολουθία της επικεφαλίδας!\n"
#define MSGTR_CannotReadMpegSequHdrEx "ΚΡΙΣΙΜΟ: Δεν είναι δυνατό να διαβαστεί η ακολουθία της επέκτασης της επικεφαλίδας!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Κακή ακολουθία της επικεφαλίδας!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Κακή ακολουθία της επέκτασης της επικεφαλίδας!\n"

#define MSGTR_ShMemAllocFail "Δεν μπορεί να προσδιοριστεί διαμοιραζόμενη μνήμη\n"
#define MSGTR_CantAllocAudioBuf "Δεν μπορεί να προσδιοριστεί buffer για έξοδο ήχου\n"
#define MSGTR_NoMemForDecodedImage "Δεν υπάρχει αρκετή μνήμη για την αποκωδικοποιημένη εικόνα στον buffer (%ld bytes)\n"

#define MSGTR_AC3notvalid "Το κανάλι AC3 δεν είναι έγκυρο.\n"
#define MSGTR_AC3only48k "Υποστηριζόμενα είναι μόνο τα κανάλια των 48000 Hz.\n"
#define MSGTR_UnknownAudio "Άγνωστο/απών format ήχου, χρήση του χωρίς-ήχο\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Αρχικοποίηση υποστήριξης του lirc...\n"
#define MSGTR_LIRCdisabled "Απενεργοποίηση της δυνατότητας χρήσης τηλεκοντρόλ\n"
#define MSGTR_LIRCopenfailed "Αποτυχία στην αρχικοποίηση της υποστήριξης του lirc!\n"
#define MSGTR_LIRCsocketerr "Υπάρχει πρόβλημα με το lirc socket: %s\n"
#define MSGTR_LIRCcfgerr "Αποτυχία κατά το διάβασμα του LIRC config αρχείου %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Περί"
#define MSGTR_FileSelect "Επιλογή αρχείου ..."
#define MSGTR_SubtitleSelect "Επιλογή υποτίτλου ..."
#define MSGTR_OtherSelect "Επιλογή ..."
#define MSGTR_MessageBox "MessageBox"
#define MSGTR_PlayList "PlayList"
#define MSGTR_SkinBrowser "Skin λίστα"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Άκυρο"
#define MSGTR_Add "Πρόσθεσε"
#define MSGTR_Remove "Αφαίρεσε"

// --- error messages ---
#define MSGTR_NEMDB "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για γράψημο στον buffer."
#define MSGTR_NEMFMR "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για την εμφάνιση του μενού."
#define MSGTR_NEMFMM "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για σχεδιασμό της μάσκας του κυρίου παραθύρου."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] error in skin config file on line %d: %s" 
#define MSGTR_SKIN_WARNING1 "[skin] warning in skin config file on line %d: widget found but before \"section\" not found ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] warning in skin config file on line %d: widget found but before \"subsection\" not found (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits or less depth bitmap not supported ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "το αρχείο ( %s ) δεν βρέθηκε\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "σφάλμα κατά την ανάγνωση του bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "σφάλμα κατά την ανάγνωση του tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "σφάλμα κατά την ανάγνωση του png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "Το RLE packed tga δεν υποστηρίζεται ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "μη αναγνωρίσιμο είδος αρχείου ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "σφάλμα κατά τη μετατροπή από 24 bit σε 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "μη αναγνωείσιμο μύνnμα: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "δεν υπάρχει αρκετή μνήμη διαθέσιμη\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "πολλαπλές πρισμένες γραμματοσειρές\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "δεν βρέθηκε αρχείο γραμματοσειράς\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "δεν βρέθηκε εικόνα του αρχελιου γραμματοσειράς\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "μη-υπαρκτό font identifier ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "μη αναγνρίσιμη παράμετρος ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[λίστα skin] δεν υπάρχει αρκετή μνήμη διαθέσιμη.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Δεν βρέθηκε skin ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "σφάλμα ανάγνωσης του skin configfile ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Περί MPlayer"
#define MSGTR_MENU_Open "Άνοιγμα ..."
#define MSGTR_MENU_PlayFile "Αναπαραγωγή αρχείου ..."
#define MSGTR_MENU_PlayVCD "Αναπαραγωγή VCD ..."
#define MSGTR_MENU_PlayDVD "Αναπαραγωγή DVD ..."
#define MSGTR_MENU_PlayURL "Αναπαραγωγή URL ..."
#define MSGTR_MENU_LoadSubtitle "Άνοιγμα υποτίτλου ..."
#define MSGTR_MENU_Playing "Αναπαραγωγή..."
#define MSGTR_MENU_Play "Αναπαραγωγή"
#define MSGTR_MENU_Pause "Παύση"
#define MSGTR_MENU_Stop "Στοπ"
#define MSGTR_MENU_NextStream "Επόμενο stream"
#define MSGTR_MENU_PrevStream "Προηγούμενο stream"
#define MSGTR_MENU_Size "Μέγεθος"
#define MSGTR_MENU_NormalSize "Κανονικό μέγεθος"
#define MSGTR_MENU_DoubleSize "Διπλάσιο μέγεθος"
#define MSGTR_MENU_FullScreen "Πλήρης οθόνη"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_PlayDisc "Αναπαραγωγή δίσκου ..."
#define MSGTR_MENU_ShowDVDMenu "Εμφάνιση του DVD μενού"
#define MSGTR_MENU_Titles "Τίτλοι"
#define MSGTR_MENU_Title "Τίτλος %2d"
#define MSGTR_MENU_None "(τίποτα)"
#define MSGTR_MENU_Chapters "Κεφάλαια"
#define MSGTR_MENU_Chapter "Κεφάλαιο %2d"
#define MSGTR_MENU_AudioLanguages "Γλώσσες ήχου"
#define MSGTR_MENU_SubtitleLanguages "Γλώσσες υποτίτλων"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Skin λίστα"
#define MSGTR_MENU_Preferences "Ρυθμίσεις"
#define MSGTR_MENU_Exit "Έξοδος ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "κρίσιμο σφάλμα ..."
#define MSGTR_MSGBOX_LABEL_Error "σφάλμα ..."
#define MSGTR_MSGBOX_LABEL_Warning "προειδοποίηση ..."

#endif
