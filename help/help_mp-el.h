// Translated by: Ioannis Panteleakis <pioann@csd.auth.gr>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2003 Arpad Gereoffy (βλέπε DOCS!)\n"
"\n";

static char help_text[]=
"Usage:   mplayer [επιλογές] [url|διαδρομή/]όνομα_αρχείου\n"
"\n"
"Βασικές επιλογές: (βλέπε manpage για ολοκληρωμένη λίστα για ΟΛΕΣ τις επιλογές!)\n"
" -vo <drv[:dev]> επιλέξτε τον οδηγό εξόδου βίντεο και τη συσκευή (βλέπε '-vo help' για τη λίστα)\n"
" -ao <drv[:dev]> επιλέξτε τον οδηγό εξόδου ήχου και τη συσκευή (βλέπε '-ao help' για τη λίστα)\n"
#ifdef HAVE_VCD
" -vcd <trackno>  αναπαραγωγή VCD (video cd) track από συσκευή αντί για αρχείο\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  ορίζει τη συσκευή DVD για πιστοποίηση (για κρυπτογραφημένους δίσκους)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  αναπαραγωγή του τίτλου/track DVD από τη συσκευή αντί για αρχείο\n"
" -alang/-slang   επιλογή της γλώσσας του ήχου/υποτίτλων του DVD (2 χαρακτήρες του κωδικού της χώρας)\n"
#endif
" -ss <timepos>   αναζήτηση σε δεδομένη θέση (δευτερόλεπτα ή hh:mm:ss)\n"
" -nosound        μη αναπαραγωγή του ήχου\n"
" -fs -vm -zoom   επιλογές για αναπαραγωγή σε πλήρη οθόνη (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   κλημάκωση εικόνας σε <x> * <y> αναλύσεις [αν ο -vo οδηγός το υποστηρίζει!]\n"
" -sub <αρχείο>   επιλογή του αρχείου υποτίτλων για χρήση (βλέπε επίσης -subfps, -subdelay)\n"
" -playlist <αρχείο> ορίζει το αρχείο της playlist\n"
" -vid x -aid y   επιλογές για επιλογή καναλιού βίντεο (x) και ήχου (y) για αναπαραγωγή\n"
" -fps x -srate y επιλογές για την αλλαγή της συχνότητας του βίντεο (x fps) και του ήχου (y Hz)\n"
" -pp <ποιότητα>  ενεργοποίηση του φίλτρου postprocessing (0-4 για DivX, 0-63 για mpegs)\n"
" -framedrop      ενεργοποίηση του frame-dropping (για αργά μηχανήματα)\n"
"\n"
"Βασικά πλήκτρα: (βλέπε manpage για μια ολοκληρωμένη λίστα, καθώς επίσης το αρχείο input.conf)\n"
" <-  ή  ->      αναζήτηση μπρος/πίσω κατά 10 δευτερόλεπτα\n"
" up ή down      αναζήτηση μπρος/πίσω κατά 1 λεπτό\n"
" pgup ή pgdown  αναζήτηση μπρος/πίσω κατά 10 λεπτά\n"
" < ή >          αναζήτηση μπρος/πίσω στην playlist\n"
" p ή SPACE      παύση ταινίας (πατήστε οποιοδήποτε πλήκτρο για να συνεχίσετε)\n"
" q ή ESC        στοπ την αναπαραγωγή και έξοδος προγράμματος\n"
" + ή -          ρύθμιση καθυστέρισης ήχου κατά +/- 0.1 δευτερόλεπτα\n"
" o               αλλαγή της OSD μεθόδου:  τίποτα / seekbar / seekbar+χρόνος\n"
" * ή /          αύξηση ή μείωση της έντασης του ήχου (πατήστε 'm' για επιλογή master/pcm)\n"
" z ή x          ρύθμιση καθυστέρισης υποτίτλων κατά +/- 0.1 δευτερόλεπτα\n"
" r ή t          ρύθμισητης θέσης των υποτίτλων πάνω/κάτω, βλέπε επίσης -vop expand !\n"
"\n"
" * * * ΒΛΕΠΕ MANPAGE ΓΙΑ ΠΕΡΙΣΣΟΤΕΡΕΣ ΛΕΠΤΟΜΕΡΕΙΕΣ, ΚΑΙ ΠΙΟ ΠΡΟΧΩΡΙΜΕΝΕΣ ΕΠΙΛΟΓΕΣ ΚΑΙ KEYS ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nΈξοδος... (%s)\n"
#define MSGTR_Exit_quit "Κλείσιμο"
#define MSGTR_Exit_eof "Τέλος του αρχείου"
#define MSGTR_Exit_error "Κρίσιμο σφάλμα"
#define MSGTR_IntBySignal "\nΤο MPlayer τερματήστηκε από το σήμα %d στο module: %s \n"
#define MSGTR_NoHomeDir "Μη δυνατή η εύρεση του HOME φακέλου\n"
#define MSGTR_GetpathProblem "get_path(\"config\") πρόβλμηα\n"
#define MSGTR_CreatingCfgFile "Δημιουργία του αρχείου config: %s\n"
#define MSGTR_InvalidVOdriver "Λάθος όνομα για τον οδηγό εξόδου βίντεο: %s\nΧρησιμοποιήστε '-vo help' για να έχετε τη λίστα των διαθέσιμων οδηγών εξόδου βίντεο.\n"
#define MSGTR_InvalidAOdriver "Λάθος όνομα για τον οδηγό εξόδου ήχου: %s\nΧρησιμοποιήστε '-ao help' για να έχετε τη λίστα των διαθέσιμων οδηγών εξόδου ήχου.\n"
#define MSGTR_CopyCodecsConf "(αντιγραφή/συντόμευση etc/codecs.conf (από τον πηγαίο του MPlayer) στο ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Χρήση του ενσωματωμένου προεπιλεγμένου codecs.conf\n"
#define MSGTR_CantLoadFont "Μη δυνατότητα φώρτωσης της γραμματοσειράς: %s\n"
#define MSGTR_CantLoadSub "Μη δυνατότητα φώρτωσης των υποτίτλων: %s\n"
#define MSGTR_ErrorDVDkey "Σφάλμα κατά την επεξεργασία του DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "Το ζητούμενο κλειδί για το DVD αποθυκεύτηκε για descrambling.\n"
#define MSGTR_DVDauthOk "Η ακολουθία πιστοποίησης του DVD φαίνεται εντάξει.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: ΚΡΙΣΙΜΟ: λοίπει το επιλεγμένο κανάλι!\n"
#define MSGTR_CantOpenDumpfile "Αδύνατο το άνοιγμα του dump αρχείου!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Μη ορισμένα FPS (ή λάθος) στο header! Χρησιμοποιήστε την επιλογή -fps!\n"
#define MSGTR_TryForceAudioFmt "Προσπάθεια να επιβολής της οικογένειας του οδηγού του codec του ήχου %d ...\n"
#define MSGTR_CantFindAfmtFallback "Δεν είναι δυνατή η εύρεση της οικογένειας του οδηγού του codec του ήχου, χρήση άλλου οδηγού.\n"
#define MSGTR_CantFindAudioCodec "Δεν είναι δυνατή η εύρεση του format του codec του ήχου 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Δοκιμάστε να αναβαθμίσεται το %s από το etc/codecs.conf\n*** Αν ακόμα υπάρχει πρόβήμα, διαβάστε DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Αδύνατη η αρχικοποίηση του codec του ήχου! -> χωρίς-ήχο\n"
#define MSGTR_TryForceVideoFmt "Προσπάθεια να επιβολής της οικογένειας του οδηγού του codec του βίντεο %d ...\n"
#define MSGTR_CantFindVideoCodec "Δεν είναι δυνατή η εύρεση του codec για τον συγκεκριμένο -vo και το format του βίντεο 0x%X !\n"
#define MSGTR_VOincompCodec "Λυπάμαι, η επιλεγμένη συσκευή video_out είναι ασύμβατη με αυτό το codec.\n"
#define MSGTR_CannotInitVO "ΚΡΙΣΙΜΟ: Αδύνατη η αρχικοποίηση του οδηγού του βίντεο!\n"
#define MSGTR_CannotInitAO "Αδύνατο το άνοιγμα/αρχικοποίηση του οδηγού του ήχου -> ΧΩΡΙΣ-ΗΧΟ\n"
#define MSGTR_StartPlaying "Εκκίνιση αναπαραγωγής...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         **************************************************************************\n"\
"         **** Το σύστημά σας είναι πολύ αργό για την αναπαραγωγή του αρχείου!  ****\n"\
"         **************************************************************************\n\n"\
"!!! Πιθανές αιτίες, προβλήματα, λύσεις: \n"\
"- Συνήθη αιτία: πρόβλημα με τον οδηγό του ήχου\n"\
"  - Δοκιμάστε -ao sdl ή χρησιμοποιήστε ALSA 0.5 ή oss προσωμοίωση του οδηγού ALSA 0.9. Διαβάστε DOCS/sound.html για περισσότερες λύσεις!\n"\
"  - Μπορείτε επίσης να πειραματηστείτε με διάφορες τιμές του -autosync, η τιμή  30 είναι μια καλή αρχή.\n"\
"- Αργή έξοδος του βίντεο\n"\
"  - Δοκιμάστε διαφορετικό -vo οδηγό (για λίστα: -vo help) ή δοκιμάστεμε -framedrop\n"\
"- Αργός επεξεργαστής\n"\
"  - Μην αναπαράγετε μεγάλα DVD/DivX αρχεία σε αργούς επεξεργαστές! Δοοκιμάστε με -hardframedrop\n"\
"- Προβληματικό αρχείο\n"\
"  - Δοκιμάστε με διάφορους συνδιασμούς από τους παρακάτω: -nobps  -ni  -mc 0  -forceidx\n"\
"- Αργά μέσα αναπαραγωγή (NFS/SMB mounts, DVD, VCD κτλ) \n"\
"  - Δοκιμάστε -cache 8192\n"\
"- Μήπως χρησιμοποιείται -cache για την αναπαραγωγή ενός non-interleaved αρχείου;\n"\
"  - Δοκιμάστε με -nocache\n"\
"Διαβάστε DOCS/video.html για ρύθμιση/επιτάχυνση του βίντεο.\n"\
"Αν κανένα από αυτά δεν βοηθάει, τότε διαβάστε DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "Το MPlayer μεταφράστηκε ΧΩΡΙΣ υποστήριξη για GUI!\n"
#define MSGTR_GuiNeedsX "Το GUI του MPlayer χρειάζεται X11!\n"
#define MSGTR_Playing "Αναπαραγωγή του %s\n"
#define MSGTR_NoSound "Ήχος: μη διαθέσιμο!!!\n"
#define MSGTR_FPSforced "Τα FPS ρυθμίστηκαν να είναι %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Μετάφραση με αυτόματη αναγνώριση επεξεργαστή - προσοχή, δεν είναι βέλτιστο! Για καλύτερες επιδόσεις, μεταφράστε το mplayer από τον πηγαίο κώδικα με --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Μετάφραση για x86 επεξεργαστή με τις ακόλουθες επεκτάσεις:"
#define MSGTR_AvailableVideoOutputPlugins "Διαθέσιμα plugins για έξοδο βίντεο:\n"
#define MSGTR_AvailableVideoOutputDrivers "Διαθέσιμοι οδηγοί για έξοδο βίντεο:\n"
#define MSGTR_AvailableAudioOutputDrivers "Διαθέσιμοι οδηγοί για έξοδο ήχου:\n"
#define MSGTR_AvailableAudioCodecs "Διαθέσιμα codecs ήχου:\n"
#define MSGTR_AvailableVideoCodecs "Διαθέσιμα codecs βίντεο:\n"
#define MSGTR_AvailableAudioFm "\nΔιαθέσιμοι (compiled-in) οδηγοί/οικογένειες codec ήχου:\n"
#define MSGTR_AvailableVideoFm "\nΔιαθέσιμοι (compiled-in) οδηγοί/οικογένειες codec βίντεο:\n"
#define MSGTR_UsingRTCTiming "Χρήση του hardware RTC του linux στα (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Βίντεο: αδύνατη η ανάγνωση ιδιοτήτων\n"
#define MSGTR_NoStreamFound "Δεν βρέθηκε κανάλι\n"
#define MSGTR_InitializingAudioCodec "Αρχικοποίηση του codec ήχου...\n"
#define MSGTR_ErrorInitializingVODevice "Σφάλμα κατά το άνοιγμα/αρχικοποίηση της επιλεγμένης video_out (-vo) συσκευή!\n"
#define MSGTR_ForcedVideoCodec "Εξαναγκασμός χρήσης του βίντεο codec: %s\n"
#define MSGTR_ForcedAudioCodec "Εξαναγκασμός χρήσης του codec ήχου: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Περιγραφή: %s\nAO: Δημιουργός: %s\n"
#define MSGTR_AOComment "AO: Σχόλιο: %s\n"
#define MSGTR_Video_NoVideo "Βίντεο: δεν υπάρχει βίντεο!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nΚΡΙΣΙΜΟ: Αδύνατη η αρχικοποίηση του φίλτρου βίντεο (-vop) ή της έξοδου βίντεο (-vo) !\n"
#define MSGTR_Paused "\n------ ΠΑΥΣΗ -------\r"
#define MSGTR_PlaylistLoadUnable "\nΑδύνατο το φώρτωμα της playlist %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- Το MPlayer κράσαρε από ένα 'Illegal Instruction'.\n"\
"  Μπορεί να είναι πρόβλημα στον νέο κώδικα για runtime CPU-αναγνώριση...\n"\
"  Παρακαλώ διαβάστε DOCS/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- Το MPlayer κράσαρε από ένα 'Illegal Instruction'.\n"\
"  Συνήθως συμβαίνει όταν τρέχεται το πρόγραμμα σε διαφορετικό επεξεργαστή από αυτόν στον οποίο έγινε\n"\
"  η μεταγλώττιση/βελτιστοποίηση.\n  Ελέγξτε το!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- Το MPlayer κράσαρε από κακή χρήση της CPU/FPU/RAM.\n"\
"  Αναμεταγλωττίστε το MPlayer με --enable-debug και τρέξτε 'gdb' backtrace και\n"\
"  disassembly. Για λεπτομέρειες, δείτε DOCS/bugreports.html#crash.b.\n"
#define MSGTR_Exit_SIGCRASH \
"- Το MPlayer κράσαρε. Αυτό δεν θα έπρεπε να είχε συμβεί.\n"\
"  Μπορεί να είναι ένα προβλημα στον κώδικα του MPlayer _ή_ στους οδηγούς σας _ή_ στην έκδοση\n"\
"  του gcc σας. Αν νομίζετε ότι φταίει το MPlayer, παρακαλώ διαβάστε DOCS/bugreports.html\n"\
"  και ακολουθήστε της οδηγίες. Δεν μπορούμε και δεν θα προσφέρουμε βοήθεια εκτός και αν στείλετε\n"\
"  τις πληροφορίες όταν αναφέρετε το πρόβλημα.\n"


// mencoder.c:

#define MSGTR_MEncoderCopyright "(C) 2000-2003 Arpad Gereoffy (βλέπε DOCS!)\n"
#define MSGTR_UsingPass3ControllFile "Χρήση του αρχείου ελέγχου pass3: %s\n"
#define MSGTR_MissingFilename "\nΠαράλειψη ονόματος αρχείου!\n\n"
#define MSGTR_CannotOpenFile_Device "Αδύνατο το άνοιγμα του αρχείου/συσκευή\n"
#define MSGTR_ErrorDVDAuth "Σφαλμα κατά την πιστοποίηση του DVD...\n"
#define MSGTR_CannotOpenDemuxer "Αδύνατο το άνοιγμα του demuxer\n"
#define MSGTR_NoAudioEncoderSelected "\nΔεν επιλέχτηκε κωδικοποιητής ήχου (-oac)! Επιλέξτε έναν ή χρησιμοποιήστε -nosound. Χρησιμοποιήστε -oac help !\n"
#define MSGTR_NoVideoEncoderSelected "\nΔεν επιλέχτηκε κωδικοποιητής βίντεο (-ovc)! Επιλέξτε έναν, Χρησιμοποιήστε -ovc help !\n"
#define MSGTR_InitializingAudioCodec "Αρχικοποίηση του codec ήχου...\n"
#define MSGTR_CannotOpenOutputFile "Αδύνατο το άνοιγμα του αρχείου εξόδου '%s'\n"
#define MSGTR_EncoderOpenFailed "Αποτυχία κατά το άνοιγμα του κωδικοποιητή\n"
#define MSGTR_ForcingOutputFourcc "Εξαναγκασμός χρήσης εξόδου fourcc σε %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Εγγραφή επικεφαλίδας του AVI...\n"
#define MSGTR_DuplicateFrames "\nδιπλασιασμός %d καρέ!!!    \n"
#define MSGTR_SkipFrame "\nπαράλειψη καρέ!!!    \n"
#define MSGTR_ErrorWritingFile "%s: σφάλμα εγγραφής αρχείου.\n"
#define MSGTR_WritingAVIIndex "\nΕγγραφή του index του AVI...\n"
#define MSGTR_FixupAVIHeader "Διόρθωση επικεφαλίδας του AVI...\n"
#define MSGTR_RecommendedVideoBitrate "Προτινόμενο bitrate του βίντεο για %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nΚανάλι βίντεο: %8.3f kbit/s  (%d bps)  μέγεθος: %d bytes  %5.3f δευτερόλεπτα  %d καρέ\n"
#define MSGTR_AudioStreamResult "\nΚανάλι ήχου: %8.3f kbit/s  (%d bps)  μέγεθος: %d bytes  %5.3f δευτερόλεπτα\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     μέθοδος μεταβλητού bitrate\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(προεπιλεγμένο)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           μέσο bitrate\n"\
"\n"\
" cbr           στεθερό bitrate\n"\
"               Αναγκάζει την κωδικοποίηση σε CBR mode σε subsequent ABR presets modes\n"\
"\n"\
" br=<0-1024>   ορισμός του bitrate σε kBit (CBR και ABR μόνο)\n"\
"\n"\
" q=<0-9>       ποιότητα (0-υψηλότερη, 9-χαμηλότερη) (μόνο για VBR)\n"\
"\n"\
" aq=<0-9>      αλγοριθμική ποιότητα (0-καλύτερο/αργό, 9-χειρότερο/γρηγορότερο)\n"\
"\n"\
" ratio=<1-100> αναλογία συμπίεσης\n"\
"\n"\
" vol=<0-10>    ορισμός του audio gain εισόδου\n"\
"\n"\
" mode=<0-3>    (προεπιλεγμένο: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: όχι\n"\
"                1: όλα\n"\
"                2: ρύθμιση\n"\
"\n"\
" fast          εναλλαγή σε γρηγορότερη κωδικοποίηση σε subsequent VBR presets modes,\n"\
"               ελαφρότερα χαμηλότερη ποιότητα και υψηλότερα bitrates.\n"\
"\n"\
" preset=<value> προσφέρει τις υψηλότερες δυνατές επιλογές ποιότητας.\n"\
"                 μεσέα: VBR  κωδικοποίηση, καλή ποιότητα\n"\
"                 (150-180 kbps εύρος bitrate)\n"\
"                 στάνταρ:  VBR κωδικοποίηση, υψηλή ποιότητα\n"\
"                 (170-210 kbps εύρος bitrate)\n"\
"                 extreme: VBR κωδικοποίηση, πολύ υψηλή ποιότητα\n"\
"                 (200-240 kbps εύρος bitrate)\n"\
"                 insane:  CBR  κωδικοποίηση, υψηλότερη preset ποιότητα\n"\
"                 (320 kbps εύρος bitrate)\n"\
"                 <8-320>: ABR κωδικοποίηση στο μέσο bitrate που δώθηκε σε kbps.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Η CD-ROM συσκευή '%s' δεν βρέθηκε!\n"
#define MSGTR_ErrTrackSelect "Σφάλμα στην επιλογή του VCD track!"
#define MSGTR_ReadSTDIN "Διαβάζοντας από το stdin...\n"
#define MSGTR_UnableOpenURL "Αδύνατο το άνοιγμα του URL: %s\n"
#define MSGTR_ConnToServer "Πραγματοποιήθηκε σύνδεση με τον server: %s\n"
#define MSGTR_FileNotFound "Το αρχείο: '%s' δεν βρέθηκε\n"

#define MSGTR_SMBInitError "Αδύνατη η αρχικοποίηση της βιβλιοθύκης libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Δεν μπόρεσα να ανοίξω από το lan: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer δεν μεταφράσηκε με υποστήρηξη διαβάσματος SMB\n"

#define MSGTR_CantOpenDVD "Δεν μπόρεσα να ανοίξω την DVD συσκευή: %s\n"
#define MSGTR_DVDwait "Ανάγνωση δομής του δίσκου, παρακαλώ περιμένετε...\n"
#define MSGTR_DVDnumTitles "Υπάρχουν %d τίτλοι στο DVD.\n"
#define MSGTR_DVDinvalidTitle "Άκυρος αριθμός για τον τίτλο του DVD: %d\n"
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
#define MSGTR_MaybeNI "(ίσως αναπαράγεται έναν non-interleaved κανάλι/αρχείο ή απέτυχε το codec)\n" \
		      "Για .AVI αρχεία, ενεργοποιήστε τη μέθοδο non-interleaved με την επιλογή -ni\n"
#define MSGTR_SwitchToNi "\nΑναγνωρίστηκε λάθος interleaved .AVI - εναλλαγή στη μέθοδο -ni!\n"
#define MSGTR_Detected_XXX_FileFormat "Αναγνωρίστηκε αρχείο τύπου %s!\n"
#define MSGTR_DetectedAudiofile "Αναγνωρίστηκε αρχείο ήχου!\n"
#define MSGTR_NotSystemStream "Μη Αναγνωρίσμιμο MPEG System Stream format... (μήπως είναι Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Μη Αναγνωρίσιμο κανάλι MPEG-ES??? Επικοινώνησε με τον author, μπορεί να είναι ένα bug :(\n"
#define MSGTR_FormatNotRecognized "============= Λυπάμαι, αυτό το είδος αρχείου δεν αναγνωρίζεται/υποστηρίζεται ===============\n"\
				  "=== Αν το αρχείο είναι ένα AVI, ASF ή MPEG κανάλι, παρακαλώ επικοινωνήστε με τον δημιουργό! ===\n"
#define MSGTR_MissingVideoStream "Δεν βρέθηκε κανάλι βίντεο!\n"
#define MSGTR_MissingAudioStream "Δεν βρέθηκε κανάλι ήχου...  ->χωρίς-ήχο\n"
#define MSGTR_MissingVideoStreamBug "Λείπει το κανάλι βίντεο!? Επικοινώνησε με τον δημιουργό, μπορεί να είναι ένα bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: το αρχείο δεν περιέχει το επιλεγμένο κανάλι ήχου ή βίντεο\n"

#define MSGTR_NI_Forced "Εξαναγκασμένο"
#define MSGTR_NI_Detected "Βρέθηκε"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI format αρχείου!\n"

#define MSGTR_UsingNINI "Χρήση ενός NON-INTERLEAVED φθαρμένου αρχείου τύπου AVI!\n"
#define MSGTR_CouldntDetFNo "Δεν μπόρεσε να διεκρυνιστεί ο αριθμός των frames (για απόλυτη αναζήτηση)  \n"
#define MSGTR_CantSeekRawAVI "Μη δυνατη αναζήτηση σε raw .AVI κανάλια! (το index είναι απαραίτητο, δοκιμάστε με την επιλογή -idx!)  \n"
#define MSGTR_CantSeekFile "Αδύνατη η αναζήτηση σε αυτό το αρχείο!  \n"

#define MSGTR_EncryptedVOB "Κωδικοποιημένο VOB αρχείο (η μετάφραση έγινε χωρίς την libcss υποστήριξη)! Διαβάστε DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Κωδικοποιημένο κανάλι αλλά δεν ζητήθηκε πιστοποίηση!!\n"

#define MSGTR_MOVcomprhdr "MOV: Συμπιεσμένα headers δεν υποστηρίζονται (ακόμα)!\n"
#define MSGTR_MOVvariableFourCC "MOV: ΠΡΟΕΙΔΟΠΟΙΗΣΗ! μεταβλητό FOURCC βρέθηκε!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Προειδοποίηση! βρέθηκαν πολλά tracks!"
#define MSGTR_FoundAudioStream "==> Βρέθηκε κανάλι ήχου: %d\n"
#define MSGTR_FoundVideoStream "==> Βρέθηκε κανάλι βίντεο: %d\n"
#define MSGTR_DetectedTV "Βρέθηκε TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Δεν είναι δυνατό το άνοιγμα του ogg demuxer\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Αναζήτηση για κανάλι ήχου (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Δεν είναι δυνατό το άνοιγμα του καναλιού ήχου: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Δεν είναι δυνατό το άνοιγμα του καναλιού υποτίτλων: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Αποτυχία κατά το άνοιγμα του demuxer ήχου: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Αποτυχία κατά το άνοιγμα του demuxer demuxer υποτίτλων: %s\n"
#define MSGTR_TVInputNotSeekable "TV input δεν είναι αναζητήσιμο! (πιθανόν η αναζήτηση να γίνει για την αλλαγή σταθμών ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Οι πληροφορίες για το demuxer %s υπάρχουν ήδη\n!"
#define MSGTR_ClipInfo "Πληροφορίες του μέσου: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Βρέθηκε progressive seq, επαναφορά σε 3:2 TELECINE mode\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: Βρέθηκε 3:2 TELECINE, ενεργοποίηση του inverse telecine fx. τα FPS άλλαξαν σε %5.3f!  \n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Αδύνατο το άνοιγμα του codec\n"
#define MSGTR_CantCloseCodec "Αδύνατο το κλείσιμο του codec\n"

#define MSGTR_MissingDLLcodec "ΣΦΑΛΜΑ: Δεν είναι δυνατό το άνοιγμα του απαιτούμενο DirectShow codec: %s\n"
#define MSGTR_ACMiniterror "Δεν είναι δυνατό να φορτωθεί/αρχικοποιηθεί το Win32/ACM codec ήχου (λείπει το DLL αρχείο?)\n"
#define MSGTR_MissingLAVCcodec "Δεν είναι δυνατό να βρεθεί το '%s' στο libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: ΚΡΙΣΙΜΟ: βρέθηκε EOF στην αναζήτηση για ακολουθία της επικεφαλίδας\n"
#define MSGTR_CannotReadMpegSequHdr "ΚΡΙΣΙΜΟ: Δεν είναι δυνατό να διαβαστεί η ακολουθία της επικεφαλίδας!\n"
#define MSGTR_CannotReadMpegSequHdrEx "ΚΡΙΣΙΜΟ: Δεν είναι δυνατό να διαβαστεί η ακολουθία της επέκτασης της επικεφαλίδας!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Κακή ακολουθία της επικεφαλίδας!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Κακή ακολουθία της επέκτασης της επικεφαλίδας!\n"

#define MSGTR_ShMemAllocFail "Δεν μπορεί να προσδιοριστεί διαμοιραζόμενη μνήμη\n"
#define MSGTR_CantAllocAudioBuf "Δεν μπορεί να προσδιοριστεί buffer για έξοδο ήχου\n"

#define MSGTR_UnknownAudio "Άγνωστο/απών format ήχου, χρήση του χωρίς-ήχο\n"

#define MSGTR_UsingExternalPP "[PP] Χρήση εξωτερικού φίλτρου προεπεξεργασίας, μέγιστο q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Χρήση φίλτρου προεπεξεργασίας για το codec, μέγιστο q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Η ιδιότητα για το βίντεο '%s' δεν υποστηρίζεται από το επιλεγμένο vo και vd! \n"
#define MSGTR_VideoCodecFamilyNotAvailable "Η αίτηση για την οικογένειας του codec βίντεο [%s] (vfm=%d) δεν διατήθεται (ενεργοποιήστε το κατά την μετάφραση του προγράμματος!)\n"
#define MSGTR_AudioCodecFamilyNotAvailable "Η αίτηση για την οικογένειας του codec ήχου [%s] (afm=%d) δεν διατήθεται (ενεργοποιήστε το κατά την μετάφραση του προγράμματος!)\n"
#define MSGTR_OpeningVideoDecoder "Άνοιγμα αποκωδικοποιητή βίντεο: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Άνοιγμα αποκωδικοποιητή ήχου: [%s] %s\n"
#define MSGTR_UninitVideo "uninit βίντεο: %d  \n"
#define MSGTR_UninitAudio "uninit ήχο: %d  \n"
#define MSGTR_VDecoderInitFailed "Αποτυχία αρχικοποίησης του VDecoder :(\n"
#define MSGTR_ADecoderInitFailed "Αποτυχία αρχικοποίησης του ADecoder :(\n"
#define MSGTR_ADecoderPreinitFailed "Αποτυχία προαρχικοποίησης του ADecoder :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Απονομή %d bytes για τον buffer εισόδου\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Απονομή %d + %d = %d bytes για τον buffer εξόδου\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Αρχικοποίηση υποστήριξης του lirc...\n"
#define MSGTR_LIRCdisabled "Απενεργοποίηση της δυνατότητας χρήσης τηλεκοντρόλ\n"
#define MSGTR_LIRCopenfailed "Αποτυχία στην αρχικοποίηση της υποστήριξης του lirc!\n"
#define MSGTR_LIRCcfgerr "Αποτυχία κατά το διάβασμα του LIRC config αρχείου %s !\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Αδύνατη εύρεση του φίλτρου βίντεο '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Αδύνατο άνοιγμα του φίλτρου βίντεο '%s'\n"
#define MSGTR_OpeningVideoFilter "Άνοιγμα του φίλτρου βίντεο: "
#define MSGTR_CannotFindColorspace "Αδύνατη εύρεση για colorspace, ακόμη και με την εισαγωγή 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: το codec δεν όρισε sh->disp_w και sh->disp_h, προσπάθεια επίλυσης!\n"
#define MSGTR_VoConfigRequest "VDec: αίτηση για επιλογής vo - %d x %d (προτινόμενο csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Δεν βρέθηκε αντίστοιχο colorspace - προσπάθεια με -vop scale...\n"
#define MSGTR_MovieAspectIsSet "Η αναλογία της ταινίας είναι %.2f:1 - προκλιμάκωση για την διόρθωση της εμφάνισης της ταινίας.\n"
#define MSGTR_MovieAspectUndefined "Η αναλογία της ταινίας δεν είναι ορισμένη - δεν εφαρμόζεται προκλιμάκωση.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Περί"
#define MSGTR_FileSelect "Επιλογή αρχείου ..."
#define MSGTR_SubtitleSelect "Επιλογή υποτίτλου ..."
#define MSGTR_OtherSelect "Επιλογή ..."
#define MSGTR_AudioFileSelect "Επιλογή εξωτερικού αρχείου ήχου ..."
#define MSGTR_FontSelect "Επιλογή γραμματοσειράς ..."
#define MSGTR_PlayList "Λίστα"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Λίστα για τα skins"
#define MSGTR_Network "Streaming δυκτίου..."
#define MSGTR_Preferences "Ιδιότητες"
#define MSGTR_OSSPreferences "Προτιμήσεις για τον οδηγό OSS"
#define MSGTR_SDLPreferences "Προτιμήσεις για τον οδηγό SDL"
#define MSGTR_NoMediaOpened "Δεν φωρτώθηκαν media"
#define MSGTR_VCDTrack "VCD track %d"
#define MSGTR_NoChapter "Μη χρήση κεφαλαίου"
#define MSGTR_Chapter "Κεφάλαιο %d"
#define MSGTR_NoFileLoaded "δεν φωρτώθηκε αρχείο"

// --- buttons ---
#define MSGTR_Ok "Εντάξει"
#define MSGTR_Cancel "Άκυρο"
#define MSGTR_Add "Πρόσθεσε"
#define MSGTR_Remove "Αφαίρεσε"
#define MSGTR_Clear "Καθάρισμα"
#define MSGTR_Config "Προτιμήσεις"
#define MSGTR_ConfigDriver "Προτίμηση οδηγού"
#define MSGTR_Browse "Αναζήτηση αρχείου"

// --- error messages ---
#define MSGTR_NEMDB "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για γράψημο στον buffer."
#define MSGTR_NEMFMR "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για την εμφάνιση του μενού."
#define MSGTR_IDFGCVD "Λυπάμαι, δεν βρέθηκε οδηγός εξόδου βίντεο που να είναι συμβατός με το GUI."
#define MSGTR_NEEDLAVCFAME "Λυπάμαι, δεν μπορείτε να αναπαράγετε αρχεία που δεν είναι mpeg με τη συσκευή DXR3/H+ χωρίς επανακωδικοποίηση.\nΠαρακαλώ ενεργοποιήστε lavc ή fame στο DXR3/H+ κουτί-διαλόγου."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] σφάλμα στο αρχείο προτιμήσεων του skin στη γραμμή %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] προειδοποίηση στο αρχείο προτιμήσεων του skin στη γραμμή %d: το widget βρέθηκε αλλά πριν το \"section\" δεν βρέθηκε ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] προειδοποίηση στο αρχείο προτιμήσεων του skin στη γραμμή %d: το widget βρέθηκε αλλά πριν το \"subsection\" δεν βρέθηκε (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] προειδοποίηση στο αρχείο προτιμήσεων του skin στη γραμμή %d: αυτό το subsection δεν υποστηρίζεται από αυτό το widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "το βάθος χρώματος εικόνας των 16 bits ή λιγότερο δεν υποστηρίζεται ( %s ).\n"
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
#define MSGTR_SKIN_FONT_FontImageNotFound "δεν βρέθηκε εικόνα του αρχείου γραμματοσειράς\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "μη-υπαρκτή η ταυτότητα της γραμματοσειράς ( %s )\n"
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
#define MSGTR_MENU_DropSubtitle "Πέταμα υποτύτλου ..."
#define MSGTR_MENU_LoadExternAudioFile "Άνοιγμα εξωτερικού αρχείου ήχου ..."
#define MSGTR_MENU_Playing "Αναπαραγωγή..."
#define MSGTR_MENU_Play "Αναπαραγωγή"
#define MSGTR_MENU_Pause "Παύση"
#define MSGTR_MENU_Stop "Στοπ"
#define MSGTR_MENU_NextStream "Επόμενο κανάλι"
#define MSGTR_MENU_PrevStream "Προηγούμενο κανάλι"
#define MSGTR_MENU_Size "Μέγεθος"
#define MSGTR_MENU_NormalSize "Κανονικό μέγεθος"
#define MSGTR_MENU_DoubleSize "Διπλάσιο μέγεθος"
#define MSGTR_MENU_FullScreen "Πλήρης οθόνη"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Αναπαραγωγή δίσκου ..."
#define MSGTR_MENU_ShowDVDMenu "Εμφάνιση του DVD μενού"
#define MSGTR_MENU_Titles "Τίτλοι"
#define MSGTR_MENU_Title "Τίτλος %2d"
#define MSGTR_MENU_None "(τίποτα)"
#define MSGTR_MENU_Chapters "Κεφάλαια"
#define MSGTR_MENU_Chapter "Κεφάλαιο %2d"
#define MSGTR_MENU_AudioLanguages "Γλώσσες ήχου"
#define MSGTR_MENU_SubtitleLanguages "Γλώσσες υποτίτλων"
#define MSGTR_MENU_PlayList "Λίστα"
#define MSGTR_MENU_SkinBrowser "Λίστα για τα skins"
#define MSGTR_MENU_Preferences "Ρυθμίσεις"
#define MSGTR_MENU_Exit "Έξοδος ..."
#define MSGTR_MENU_Mute "Απενεργοποίηση ήχου"
#define MSGTR_MENU_Original "Αρχικό"
#define MSGTR_MENU_AspectRatio "Αναλογία εμφάνισης"
#define MSGTR_MENU_AudioTrack "Track ήχου"
#define MSGTR_MENU_Track "Track %d"
#define MSGTR_MENU_VideoTrack "Track βίντεο"

// --- equalizer
#define MSGTR_EQU_Audio "Ήχος"
#define MSGTR_EQU_Video "Βίντεο"
#define MSGTR_EQU_Contrast "Contrast: "
#define MSGTR_EQU_Brightness "Φωτεινότητα: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturation: "
#define MSGTR_EQU_Front_Left "Μπροστά Αριστερά"
#define MSGTR_EQU_Front_Right "Μπροστά Δεξιά"
#define MSGTR_EQU_Back_Left "Πίσω αριστερά"
#define MSGTR_EQU_Back_Right "Πίσω δεξιά"
#define MSGTR_EQU_Center "Κέντρο"
#define MSGTR_EQU_Bass "Bass"
#define MSGTR_EQU_All "Όλα"
#define MSGTR_EQU_Channel1 "Κανάλι 1:"
#define MSGTR_EQU_Channel2 "Κανάλι 2:"
#define MSGTR_EQU_Channel3 "Κανάλι 3:"
#define MSGTR_EQU_Channel4 "Κανάλι 4:"
#define MSGTR_EQU_Channel5 "Κανάλι 5:"
#define MSGTR_EQU_Channel6 "Κανάλι 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Διαδρομή"
#define MSGTR_PLAYLIST_Selected "Επιλεγμένα αρχεία"
#define MSGTR_PLAYLIST_Files "Αρχεία"
#define MSGTR_PLAYLIST_DirectoryTree "Δένδρο καταλόγων"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Ήχος"
#define MSGTR_PREFERENCES_Video "Βίντεο"
#define MSGTR_PREFERENCES_SubtitleOSD "Υπότιτλοι και OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs και demuxer"
#define MSGTR_PREFERENCES_Misc "Διάφορα"

#define MSGTR_PREFERENCES_None "Τίποτα"
#define MSGTR_PREFERENCES_AvailableDrivers "Διαθέσιμοι οδηγοί:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Μη-αναπαραγωγή ήχου"
#define MSGTR_PREFERENCES_NormalizeSound "Κανονικοποίηση ήχου"
#define MSGTR_PREFERENCES_EnEqualizer "Ενεργοποίηση του equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Ενεργοποίηση του extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficient:"
#define MSGTR_PREFERENCES_AudioDelay "Καθυστέριση ήχου"
#define MSGTR_PREFERENCES_DoubleBuffer "Ενεργοποίηση double buffering"
#define MSGTR_PREFERENCES_DirectRender "Ενεργοποίηση direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Ενεργοποίηση πετάματος καρέ"
#define MSGTR_PREFERENCES_HFrameDrop "Ενεργοποίηση ΣΚΛΗΡΗΣ κατάργησης καρέ (επικίνδυνο)"
#define MSGTR_PREFERENCES_Flip "Flip της εικόνας πάνω-κάτω"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Μετρητής χρόνου και δείκτες"
#define MSGTR_PREFERENCES_OSDProgress "Μόνο Progressbars"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Χρόνος, ποσοστό επί της εκατό και συνολικός χρόνος"
#define MSGTR_PREFERENCES_Subtitle "Υπότιτλος:"
#define MSGTR_PREFERENCES_SUB_Delay "Καθυστέριση: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Θέση: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Απενεργοποίηση αυτόματου φωρτώματος υποτίτλων"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode υπότιτλος"
#define MSGTR_PREFERENCES_SUB_MPSUB "Μετατροπή εισαγόμενου υπότιτλου σε υπότιτλο τύπου MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Μετατροπή εισαγόμενου υπότιτλου σε τύπο SubViewer( SRT ) χρόνο-βασιζόμενο"
#define MSGTR_PREFERENCES_SUB_Overlap "Εναλλαγή του overlapping υποτίτλου"
#define MSGTR_PREFERENCES_Font "Γραμματοσειρά:"
#define MSGTR_PREFERENCES_FontFactor "Παράγοντας της γραμματοσειράς:"
#define MSGTR_PREFERENCES_PostProcess "Ενεργοποίηση προεπεξεργασίας"
#define MSGTR_PREFERENCES_AutoQuality "Αυτόματη ποιότητα: "
#define MSGTR_PREFERENCES_NI "Χρήση του non-interleaved AVI parser"
#define MSGTR_PREFERENCES_IDX "Αναδημιουργία του πίνακα index, αν χρειάζεται"
#define MSGTR_PREFERENCES_VideoCodecFamily "Οικογένεια του βίντεο codec:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Οικογένεια του codec ήχου:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Επίπεδο OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Υπότιτλος"
#define MSGTR_PREFERENCES_FRAME_Font "Γραμματοσειρά"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Προεπεξεργασία"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec και demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_FRAME_Misc "Διάφορα"
#define MSGTR_PREFERENCES_OSS_Device "Συσκευή:"
#define MSGTR_PREFERENCES_OSS_Mixer "Μίκτης:"
#define MSGTR_PREFERENCES_SDL_Driver "Οδηγός:"
#define MSGTR_PREFERENCES_Message "Προσοχή, μερικές λειτουργίες χρειάζονται επανεκκίνιση αναπαραγωγής."
#define MSGTR_PREFERENCES_DXR3_VENC "Κωδικοποιητής βίντεο:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Χρήση του LAVC (ffmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Χρήση του FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Δυτικές Ευρωπαϊκές γλώσσες (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Δυτικές Ευρωπαϊκές γλώσσες με Ευρώ (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slavic/Central European Languages (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galician, Maltese, Τούρκικα (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Παλιά Baltic κωδικοσειρά (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Κυριλλικά (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Αραβικά (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Νέα Ελληνικά (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Τούρκικα (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltic (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Κέλτικα (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Εβραϊκά (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Ρώσικα (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainian, Belarusian (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Απλοποιημένα Κινέζικα (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Παραδοσιακά Κινέζικα (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Ιαπονέζικα (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Κορεάτικα (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai κωδικοσειρά (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrillic Windows (CP1251)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Όχι αυτόματη κλιμάκωση"
#define MSGTR_PREFERENCES_FontPropWidth "Αναλογία με το πλάτος της ταινίας"
#define MSGTR_PREFERENCES_FontPropHeight "Αναλογία με το ύψος της ταινίας"
#define MSGTR_PREFERENCES_FontPropDiagonal "Αναλογία με τη διαγώνιο της ταινίας"
#define MSGTR_PREFERENCES_FontEncoding "Κωδικοποίηση:"
#define MSGTR_PREFERENCES_FontBlur "Blur:"
#define MSGTR_PREFERENCES_FontOutLine "Outline:"
#define MSGTR_PREFERENCES_FontTextScale "Κλιμάκωση του Κειμένου:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD κλιμάκωση:"
#define MSGTR_PREFERENCES_Cache "Ενεργοποίηση/απενεργοποίηση της cache"
#define MSGTR_PREFERENCES_CacheSize "Μέγεθος της cache: "
#define MSGTR_PREFERENCES_LoadFullscreen "Εκκίνιση σε πλήρης οθόνη"
#define MSGTR_PREFERENCES_CacheSize "Μέγεθος της cache: "
#define MSGTR_PREFERENCES_XSCREENSAVER "Απενεργοποίηση του XScreenSaver"
#define MSGTR_PREFERENCES_AutoSync "Ενεργοποίηση/απενεργοποίηση του αυτόματου συγχρονισμού"
#define MSGTR_PREFERENCES_AutoSyncValue "Αυτόματος συγχρονισμός: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM συσκευή:"
#define MSGTR_PREFERENCES_DVDDevice "DVD συσκευή:"
#define MSGTR_PREFERENCES_FPS "FPS ταινίας:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Εμφάνηση του Video Window όταν δεν είναι ενεργοποιημένο"

#define MSGTR_ABOUT_UHU "Η ανάπτυξη του GUI προωθείται από την UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   Κύρια ομάδα του MPlayer:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Επιπλέον προγραμματιστές:\n"
#define MSGTR_ABOUT_MainTesters "   Βασικοί testers:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Κρίσιμο σφάλμα ..."
#define MSGTR_MSGBOX_LABEL_Error "Σφάλμα ..."
#define MSGTR_MSGBOX_LABEL_Warning "Προειδοποίηση ..."

#endif
