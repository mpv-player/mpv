// sync'ed with help_mp-en.h 1.121
// Translated by: Ioannis Panteleakis <pioann@csd.auth.gr>
// Various corrections and additions by: Haris Kouzinopoulos <ironhell3@hotmail.com>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Usage:   mplayer [επιλογές] [url|διαδρομή/]όνομα_αρχείου\n"
"\n"
"Βασικές επιλογές: (Ανατρέξτε στη  σελίδα εγχειριδίου για ολοκληρωμένη λίστα με επιλογές)\n"
" -vo <drv[:dev]> επιλέξτε τον οδηγό εξόδου βίντεο και τη συσκευή (βλέπε '-vo help' για τη λίστα)\n"
" -ao <drv[:dev]> επιλέξτε τον οδηγό εξόδου ήχου και τη συσκευή (βλέπε '-ao help' για τη λίστα)\n"
#ifdef HAVE_VCD
" vcd://<αριθμός track>  αναπαραγωγή track VCD (video cd)  από συσκευή αντί για αρχείο\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  αναπαραγωγή του τίτλου/track DVD από τη συσκευή αντί για αρχείο\n"
" -alang/-slang   επιλογή της γλώσσας του ήχου/υποτίτλων του DVD (2 χαρακτήρες του κωδικού της χώρας)\n"
#endif
" -ss <timepos>   αναζήτηση σε δεδομένη θέση (δευτερόλεπτα ή ωω:λλ:δδ)\n"
" -nosound        μη αναπαραγωγή του ήχου\n"
" -fs   επιλογές για αναπαραγωγή σε πλήρη οθόνη (ή -vm -zoom, δείτε man page για περισσότερες πληροφορίες)\n"
" -x <x> -y <y>   κλιμάκωση εικόνας σε <x> * <y> αναλύσεις [αν ο -vo οδηγός το υποστηρίζει!]\n"
" -sub <αρχείο>   επιλογή του αρχείου υποτίτλων για χρήση (βλέπε επίσης -subfps, -subdelay)\n"
" -playlist <αρχείο> ορίζει το αρχείο της λίστας αναπαραγωγής\n"
" -vid x -aid y   επιλογή καναλιού βίντεο (x) και ήχου (y) για αναπαραγωγή\n"
" -fps x -srate y επιλογή  συχνότητας του βίντεο (x fps) και  ήχου (y Hz)\n"
" -pp <ποιότητα>  ενεργοποίηση του φίλτρου postprocessing (0-4 για DivX, 0-63 για mpeg)\n"
" -framedrop      ενεργοποίηση του frame-dropping (για αργά μηχανήματα)\n"
"\n"
"Βασικά πλήκτρα: (Ανατρέξτε στη  σελίδα εγχειριδίου για ολοκληρωμένη λίστα , καθώς επίσης και  στο αρχείο input.conf)\n"
" <-  ή  ->      αναζήτηση μπρος/πίσω κατά 10 δευτερόλεπτα\n"
" up ή down      αναζήτηση μπρος/πίσω κατά 1 λεπτό\n"
" pgup ή pgdown  αναζήτηση μπρος/πίσω κατά 10 λεπτά\n"
" < ή >          αναζήτηση μπρος/πίσω στην λίστα αναπαραγωγής\n"
" p ή SPACE      παύση ταινίας (πατήστε οποιοδήποτε πλήκτρο για να συνεχίσετε)\n"
" q ή ESC        στοπ την αναπαραγωγή και έξοδος προγράμματος\n"
" + ή -          ρύθμιση καθυστέρησης ήχου κατά +/- 0.1 δευτερόλεπτα\n"
" o               αλλαγή της OSD μεθόδου:  τίποτα / μπάρα προόδου / μπάρα προόδου+χρόνος\n"
" * ή /          αύξηση ή μείωση της έντασης του ήχου (πατήστε 'm' για επιλογή master/pcm)\n"
" z ή x          ρύθμιση καθυστέρησης υποτίτλων κατά +/- 0.1 δευτερόλεπτα\n"
" r ή t          ρύθμιση της θέσης των υποτίτλων πάνω/κάτω, βλέπε επίσης -vf expand!\n"
"\n"
" Ανατρέξτε στη σελίδα εγχειριδίου για περισσότερες λεπτομέρειες, πιο προχωρημένες επιλογές και  λίστα με πλήκτρα \n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\n Έξοδος...\n"
#define MSGTR_ExitingHow "\n Έξοδος... (%s)\n"
#define MSGTR_Exit_quit "Κλείσιμο"
#define MSGTR_Exit_eof "Τέλος του αρχείου"
#define MSGTR_Exit_error "Κρίσιμο σφάλμα"
#define MSGTR_IntBySignal "\n Το MPlayer τερματίστηκε από το σήμα %d στο module: %s \n"
#define MSGTR_NoHomeDir "Μη δυνατή η εύρεση του HOME φακέλου\n"
#define MSGTR_GetpathProblem "get_path(\"config\") πρόβλημα\n"
#define MSGTR_CreatingCfgFile "Δημιουργία του αρχείου config: %s\n"
#define MSGTR_BuiltinCodecsConf "Χρήση του ενσωματωμένου προεπιλεγμένου codecs.conf\n"
#define MSGTR_CantLoadFont "Μη δυνατότητα φόρτωσης της γραμματοσειράς: %s\n"
#define MSGTR_CantLoadSub "Μη δυνατότητα φόρτωσης των υποτίτλων: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: Σφάλμα: λείπει το επιλεγμένο κανάλι!\n"
#define MSGTR_CantOpenDumpfile "Αδύνατο το άνοιγμα του dump αρχείου!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Μη ορισμένα FPS (ή λάθος) στην επικεφαλίδα! Χρησιμοποιήστε την επιλογή -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Προσπάθεια  επιβολής της οικογένειας του οδηγού του ήχου %s...\n"
#define MSGTR_CantFindAudioCodec "Δεν είναι δυνατή η εύρεση του format του οδηγού του ήχου 0x%X!\n"
#define MSGTR_RTFMCodecs "Διαβάστε DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Προσπάθεια  επιβολής της οικογένειας του οδηγού του βίντεο %s...\n"
#define MSGTR_CantFindVideoCodec "Δεν είναι δυνατή η εύρεση του οδηγού  για τον συγκεκριμένο -vo και το format του βίντεο 0x%X!\n"
#define MSGTR_VOincompCodec "Λυπάμαι, η επιλεγμένη συσκευή video_out δεν είναι συμβατή με αυτό τον οδηγό.\n"
#define MSGTR_CannotInitVO "Σφάλμα: Αδύνατη η αρχικοποίηση του οδηγού του βίντεο!\n"
#define MSGTR_CannotInitAO "Αδύνατο το άνοιγμα/αρχικοποίηση του οδηγού του ήχου -> ΧΩΡΙΣ-ΗΧΟ\n"
#define MSGTR_StartPlaying "Εκκίνηση αναπαραγωγής...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         **************************************************************************\n"\
"         **** Το σύστημά σας είναι πολύ αργό για την αναπαραγωγή του αρχείου!  ****\n"\
"         **************************************************************************\n\n"\
"Πιθανές αιτίες, προβλήματα, λύσεις: \n"\
"- Συνήθη αιτία: πρόβλημα με τον οδηγό του ήχου\n"\
"  - Δοκιμάστε -ao sdl ή χρησιμοποιήστε ALSA 0.5 ή oss προσομοίωση του οδηγού ALSA 0.9.\n"\
"  - Μπορείτε επίσης να πειραματιστείτε με διάφορες τιμές του -autosync, η τιμή  30 είναι μια καλή αρχή.\n"\
"- Αργή έξοδος του βίντεο\n"\
"  - Δοκιμάστε διαφορετικό -vo οδηγό (για λίστα: -vo help) ή δοκιμάστε με -framedrop\n"\
"- Αργός επεξεργαστής\n"\
"  - Μην αναπαράγετε μεγάλα DVD/DivX αρχεία σε αργούς επεξεργαστές! Δοκιμάστε με -hardframedrop\n"\
"- Προβληματικό αρχείο\n"\
"  - Δοκιμάστε με διάφορους συνδυασμούς από τους παρακάτω: -nobps  -ni  -mc 0  -forceidx\n"\
"- Αργά μέσα αναπαραγωγή (NFS/SMB mounts, DVD, VCD κτλ) \n"\
"  - Δοκιμάστε -cache 8192\n"\
"- Μήπως χρησιμοποιείται -cache για την αναπαραγωγή ενός non-interleaved αρχείου;\n"\
"  - Δοκιμάστε με -nocache\n"\
"Διαβάστε το DOCS/HTML/en/video.html για ρύθμιση/επιτάχυνση του βίντεο.\n"\
"Αν κανένα από αυτά δεν βοηθάει, τότε διαβάστε το DOCS/HTML/en/bugreports.html!\n\n"

#define MSGTR_NoGui "Το MPlayer μεταφράστηκε ΧΩΡΙΣ υποστήριξη για GUI!\n"
#define MSGTR_GuiNeedsX "Το GUI του MPlayer χρειάζεται X11!\n"
#define MSGTR_Playing "Αναπαραγωγή του %s\n"
#define MSGTR_NoSound "Ήχος: μη διαθέσιμο!!!\n"
#define MSGTR_FPSforced "Τα FPS ρυθμίστηκαν να είναι %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Μετάφραση με αυτόματη αναγνώριση επεξεργαστή - προσοχή, δεν είναι βέλτιστο! Για καλύτερες επιδόσεις, μεταφράστε το mplayer από τον πηγαίο κώδικα με --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Μετάφραση για x86 επεξεργαστή με τις ακόλουθες επεκτάσεις:"
#define MSGTR_AvailableVideoOutputDrivers "Διαθέσιμοι οδηγοί για έξοδο βίντεο:\n"
#define MSGTR_AvailableAudioOutputDrivers "Διαθέσιμοι οδηγοί για έξοδο ήχου:\n"
#define MSGTR_AvailableAudioCodecs "Διαθέσιμα codecs ήχου:\n"
#define MSGTR_AvailableVideoCodecs "Διαθέσιμα codecs βίντεο:\n"
#define MSGTR_AvailableAudioFm "\nΔιαθέσιμοι (compiled-in) οδηγοί/οικογένειες codec ήχου:\n"
#define MSGTR_AvailableVideoFm "\nΔιαθέσιμοι (compiled-in) οδηγοί/οικογένειες codec βίντεο:\n"
#define MSGTR_AvailableFsType "Διαθέσιμα επίπεδα αλλαγής σε πλήρη οθόνη:\n"
#define MSGTR_UsingRTCTiming "Χρήση του hardware RTC του linux στα (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Βίντεο: αδύνατη η ανάγνωση ιδιοτήτων\n"
#define MSGTR_NoStreamFound "Δεν βρέθηκε κανάλι\n"
#define MSGTR_ErrorInitializingVODevice "Σφάλμα κατά το άνοιγμα/αρχικοποίηση της επιλεγμένης video_out (-vo) συσκευή!\n"
#define MSGTR_ForcedVideoCodec "Εξαναγκασμός χρήσης του βίντεο codec: %s\n"
#define MSGTR_ForcedAudioCodec "Εξαναγκασμός χρήσης του codec ήχου: %s\n"
#define MSGTR_Video_NoVideo "Βίντεο: δεν υπάρχει βίντεο!!!\n"
#define MSGTR_NotInitializeVOPorVO "\n Σφάλμα: Αδύνατη η αρχικοποίηση του φίλτρου βίντεο (-vf) ή της εξόδου βίντεο (-vo)!\n"
#define MSGTR_Paused "\n  =====  ΠΑΥΣΗ  =====\r"
#define MSGTR_PlaylistLoadUnable "\n Αδύνατη η φόρτωση  της λίστας αναπαραγωγής %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- Το MPlayer κατέρρευσε από ένα 'Illegal Instruction'.\n"\
"  Μπορεί να είναι πρόβλημα στον νέο κώδικα για runtime CPU-αναγνώριση...\n"\
"  Παρακαλούμε διαβάστε το DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- Το MPlayer κατέρρευσε από ένα 'Illegal Instruction'.\n"\
"  Συνήθως συμβαίνει όταν τρέχετε το πρόγραμμα σε διαφορετικό επεξεργαστή από αυτόν στον οποίο έγινε\n"\
"  η μεταγλώττιση/βελτιστοποίηση.\n"\
"  Ελέγξτε το!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- Το MPlayer κατέρρευσε  από κακή χρήση του επεξεργαστή ή της μνήμης.\n"\
"  Αναμεταγλωττίστε το MPlayer με --enable-debug και τρέξτε 'gdb' backtrace και\n"\
"  disassembly. Για λεπτομέρειες, δείτε το DOCS/HTML/en/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- Το MPlayer κατέρρευσε. Αυτό δεν θα έπρεπε να είχε συμβεί.\n"\
"  Μπορεί να είναι ένα πρόβλημα στον κώδικα του MPlayer _ή_ στους οδηγούς σας _ή_ στην έκδοση\n"\
"  του gcc σας. Αν νομίζετε ότι φταίει το MPlayer, παρακαλώ διαβάστε το DOCS/HTML/en/bugreports.html\n"\
"  και ακολουθήστε της οδηγίες. Δεν μπορούμε και δεν θα προσφέρουμε βοήθεια εκτός και αν στείλετε\n"\
"  τις πληροφορίες όταν αναφέρετε το πρόβλημα.\n"


// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Χρήση του αρχείου ελέγχου pass3: %s\n"
#define MSGTR_MissingFilename "\n Παράλειψη ονόματος αρχείου!\n\n"
#define MSGTR_CannotOpenFile_Device "Αδύνατο το άνοιγμα του αρχείου/συσκευή\n"
#define MSGTR_CannotOpenDemuxer "Αδύνατο το άνοιγμα του demuxer\n"
#define MSGTR_NoAudioEncoderSelected "\n Δεν επιλέχτηκε κωδικοποιητής ήχου (-oac)! Επιλέξτε έναν ή χρησιμοποιήστε -nosound. Χρησιμοποιήστε -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\n Δεν επιλέχτηκε κωδικοποιητής βίντεο (-ovc)! Επιλέξτε έναν, Χρησιμοποιήστε -ovc help!\n"
#define MSGTR_CannotOpenOutputFile "Αδύνατο το άνοιγμα του αρχείου εξόδου '%s'\n"
#define MSGTR_EncoderOpenFailed "Αποτυχία κατά το άνοιγμα του κωδικοποιητή \n"
#define MSGTR_ForcingOutputFourcc "Εξαναγκασμός χρήσης εξόδου fourcc σε %x [%.4s]\n"
#define MSGTR_DuplicateFrames "\n διπλασιασμός %d καρέ!!!    \n"
#define MSGTR_SkipFrame "\n παράλειψη καρέ!!!    \n"
#define MSGTR_ErrorWritingFile "%s: σφάλμα εγγραφής αρχείου.\n"
#define MSGTR_RecommendedVideoBitrate "Προτεινόμενο bitrate του βίντεο για %s CD: %d\n"
#define MSGTR_VideoStreamResult "\n Κανάλι βίντεο: %8.3f kbit/s  (%d B/s)  μέγεθος: %"PRIu64" bytes  %5.3f δευτερόλεπτα  %d καρέ\n"
#define MSGTR_AudioStreamResult "\n Κανάλι ήχου: %8.3f kbit/s  (%d B/s)  μέγεθος: %"PRIu64" bytes  %5.3f δευτερόλεπτα\n"

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
" cbr           σταθερό bitrate\n"\
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
"                 μεσαία: VBR  κωδικοποίηση, καλή ποιότητα\n"\
"                 (150-180 kbps εύρος bitrate)\n"\
"                 στάνταρ:  VBR κωδικοποίηση, υψηλή ποιότητα\n"\
"                 (170-210 kbps εύρος bitrate)\n"\
"                 extreme: VBR κωδικοποίηση, πολύ υψηλή ποιότητα\n"\
"                 (200-240 kbps εύρος bitrate)\n"\
"                 insane:  CBR  κωδικοποίηση, υψηλότερη preset ποιότητα\n"\
"                 (320 kbps εύρος bitrate)\n"\
"                 <8-320>: ABR κωδικοποίηση στο μέσο bitrate που δόθηκε σε kbps.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Η CD-ROM συσκευή '%s' δεν βρέθηκε!\n"
#define MSGTR_ErrTrackSelect "Σφάλμα στην επιλογή του VCD track!"
#define MSGTR_ReadSTDIN "Διαβάζοντας από το stdin...\n"
#define MSGTR_UnableOpenURL "Αδύνατο το άνοιγμα του URL: %s\n"
#define MSGTR_ConnToServer "Πραγματοποιήθηκε σύνδεση με τον server: %s\n"
#define MSGTR_FileNotFound "Το αρχείο: '%s' δεν βρέθηκε\n"

#define MSGTR_SMBInitError "Αδύνατη η αρχικοποίηση της βιβλιοθήκης libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Δεν μπόρεσα να ανοίξω από το τοπικό δίκτυο: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer δεν μεταφράστηκε με υποστήριξη ανάγνωσης SMB\n"

#define MSGTR_CantOpenDVD "Δεν μπόρεσα να ανοίξω την συσκευή DVD: %s\n"
#define MSGTR_DVDnumTitles "Υπάρχουν %d τίτλοι στο DVD.\n"
#define MSGTR_DVDinvalidTitle "Άκυρος αριθμός για τον τίτλο του DVD: %d\n"
#define MSGTR_DVDnumChapters "Υπάρχουν %d κεφάλαια σε αυτόν τον τίτλο του DVD.\n"
#define MSGTR_DVDinvalidChapter "Λάθος αριθμός των κεφαλαίων του DVD: %d\n"
#define MSGTR_DVDnumAngles "Υπάρχουν %d γωνίες σε αυτό τον τίτλο του DVD.\n"
#define MSGTR_DVDinvalidAngle "Λάθος αριθμός των γωνιών του DVD: %d\n"
#define MSGTR_DVDnoIFO "Δεν είναι δυνατό το άνοιγμα του IFO αρχείο για τον τίτλο του DVD %d.\n"
#define MSGTR_DVDnoVOBs "Δεν είναι δυνατό το άνοιγμα των VOB (VTS_%02d_1.VOB).\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Προειδοποίηση! Η επικεφαλίδα του καναλιού ήχου %d ορίζεται ξανά!\n"
#define MSGTR_VideoStreamRedefined "Προειδοποίηση! Η επικεφαλίδα του καναλιού βίντεο %d ορίζεται ξανά!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Πολλαπλά (%d σε %d bytes) πακέτα ήχου στον buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Πολλαπλά (%d σε %d bytes) πακέτα βίντεο στον buffer!\n"
#define MSGTR_MaybeNI "(ίσως αναπαράγεται έναν non-interleaved κανάλι/αρχείο ή απέτυχε το codec)\n" \
		      "Για .AVI αρχεία, ενεργοποιήστε τη μέθοδο non-interleaved με την επιλογή -ni\n"
#define MSGTR_SwitchToNi "\n Αναγνωρίστηκε λάθος interleaved .AVI - εναλλαγή στη μέθοδο -ni!\n"
#define MSGTR_Detected_XXX_FileFormat "Αναγνωρίστηκε αρχείο τύπου %s!\n"
#define MSGTR_DetectedAudiofile "Αναγνωρίστηκε αρχείο ήχου!\n"
#define MSGTR_NotSystemStream "Μη Αναγνωρίσιμο MPEG System Stream format... (μήπως είναι Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Μη Αναγνωρίσιμο κανάλι MPEG-ES??? Επικοινώνησε με τον δημιουργό, μπορεί να είναι ένα bug :(\n"
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
#define MSGTR_CouldntDetFNo "Δεν μπόρεσε να διευκρινιστεί ο αριθμός των frames (για απόλυτη αναζήτηση)  \n"
#define MSGTR_CantSeekRawAVI "Μη δυνατή αναζήτηση σε raw .AVI κανάλια! (το index είναι απαραίτητο, δοκιμάστε με την επιλογή -idx!)  \n"
#define MSGTR_CantSeekFile "Αδύνατη η αναζήτηση σε αυτό το αρχείο!  \n"

#define MSGTR_MOVcomprhdr "MOV: Συμπιεσμένες επικεφαλίδες δεν υποστηρίζονται (ακόμα)!\n"
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
#define MSGTR_OpeningSubtitlesDemuxerFailed "Αποτυχία κατά το άνοιγμα του demuxer υποτίτλων: %s\n"
#define MSGTR_TVInputNotSeekable "TV input δεν είναι αναζητήσιμο! (πιθανόν η αναζήτηση να γίνει για την αλλαγή σταθμών ;)\n"
#define MSGTR_ClipInfo "Πληροφορίες του μέσου: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: βρέθηκε περιεχόμενο 30fps NTSC, αλλαγή του framerate.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: βρέθηκε περιεχόμενο 24fps progressive NTSC, αλλαγή του framerate.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Αδύνατο το άνοιγμα του codec\n"
#define MSGTR_CantCloseCodec "Αδύνατο το κλείσιμο του codec\n"

#define MSGTR_MissingDLLcodec "Σφάλμα: Δεν είναι δυνατό το άνοιγμα του απαιτούμενο DirectShow codec: %s\n"
#define MSGTR_ACMiniterror "Δεν είναι δυνατό να φορτωθεί/αρχικοποιηθεί το Win32/ACM codec ήχου (λείπει το DLL αρχείο?)\n"
#define MSGTR_MissingLAVCcodec "Δεν είναι δυνατό να βρεθεί το '%s' στο libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: Σφάλμα: βρέθηκε τέλος αρχείου στην αναζήτηση για ακολουθία της επικεφαλίδας\n"
#define MSGTR_CannotReadMpegSequHdr "Σφάλμα: Δεν είναι δυνατό να διαβαστεί η ακολουθία της επικεφαλίδας!\n"
#define MSGTR_CannotReadMpegSequHdrEx "Σφάλμα: Δεν είναι δυνατό να διαβαστεί η ακολουθία της επέκτασης της επικεφαλίδας!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Κακή ακολουθία της επικεφαλίδας!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Κακή ακολουθία της επέκτασης της επικεφαλίδας!\n"

#define MSGTR_ShMemAllocFail "Δεν μπορεί να προσδιοριστεί διαμοιραζόμενη μνήμη\n"
#define MSGTR_CantAllocAudioBuf "Δεν μπορεί να προσδιοριστεί buffer για έξοδο ήχου\n"

#define MSGTR_UnknownAudio "Άγνωστο/απών format ήχου, χρήση του χωρίς-ήχο\n"

#define MSGTR_UsingExternalPP "[PP] Χρήση εξωτερικού φίλτρου προεπεξεργασίας, μέγιστο q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Χρήση φίλτρου προεπεξεργασίας για το codec, μέγιστο q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Η ιδιότητα για το βίντεο '%s' δεν υποστηρίζεται από το επιλεγμένο vo και vd! \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Η αίτηση για την οικογένεια του codec βίντεο [%s] (vfm=%s) δεν διατίθεται (ενεργοποιήστε το κατά την μετάφραση του προγράμματος!)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Η αίτηση για την οικογένεια του codec ήχου [%s] (afm=%s) δεν διατίθεται (ενεργοποιήστε το κατά την μετάφραση του προγράμματος!)\n"
#define MSGTR_OpeningVideoDecoder "Άνοιγμα αποκωδικοποιητή βίντεο: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Άνοιγμα αποκωδικοποιητή ήχου: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit βίντεο: %s  \n"
#define MSGTR_UninitAudioStr "uninit ήχο: %s  \n"
#define MSGTR_VDecoderInitFailed "Αποτυχία αρχικοποίησης του VDecoder :(\n"
#define MSGTR_ADecoderInitFailed "Αποτυχία αρχικοποίησης του ADecoder :(\n"
#define MSGTR_ADecoderPreinitFailed "Αποτυχία προαρχικοποίησης του ADecoder :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Απονομή %d bytes για τον buffer εισόδου\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Απονομή %d + %d = %d bytes για τον buffer εξόδου\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Αρχικοποίηση υποστήριξης του lirc...\n"
#define MSGTR_LIRCopenfailed "Αποτυχία στην αρχικοποίηση της υποστήριξης του lirc!\n"
#define MSGTR_LIRCcfgerr "Αποτυχία κατά το διάβασμα του αρχείου παραμέτρων του lirc %s!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Αδύνατη η εύρεση του φίλτρου βίντεο '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Αδύνατο το άνοιγμα του φίλτρου βίντεο '%s'\n"
#define MSGTR_OpeningVideoFilter "Άνοιγμα του φίλτρου βίντεο: "
#define MSGTR_CannotFindColorspace "Αδύνατη εύρεση για colorspace, ακόμη και με την εισαγωγή 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: το codec δεν όρισε sh->disp_w και sh->disp_h, προσπάθεια επίλυσης!\n"
#define MSGTR_VoConfigRequest "VDec: αίτηση για επιλογή vo - %d x %d (προτεινόμενο csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Δεν βρέθηκε αντίστοιχο colorspace - προσπάθεια με -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Η αναλογία της ταινίας είναι %.2f:1 - προκλιμάκωση για την διόρθωση της εμφάνισης της ταινίας.\n"
#define MSGTR_MovieAspectUndefined "Η αναλογία της ταινίας δεν είναι ορισμένη - δεν εφαρμόζεται προκλιμάκωση.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Θα πρέπει να αναβαθμήσετε ή να εγκαταστήσετε το πακέτο με τα codecs.\nΔείτε τη διεύθυνση http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "Πληροφορία: το βίντεο codec Win32/DShow αρχικοποιήθηκε επιτυχώς.\n"
#define MSGTR_DMOInitOK "Πληροφορία: το βίντεο codec Win32/DMO αρχικοποιήθηκε επιτυχώς.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Αδύνατη η αποστολή του γεγονότος EWMH fullscreen!\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Περί"
#define MSGTR_FileSelect "Επιλογή αρχείου..."
#define MSGTR_SubtitleSelect "Επιλογή υποτίτλου..."
#define MSGTR_OtherSelect "Επιλογή..."
#define MSGTR_AudioFileSelect "Επιλογή εξωτερικού αρχείου ήχου..."
#define MSGTR_FontSelect "Επιλογή γραμματοσειράς..."
#define MSGTR_PlayList "Λίστα Αναπαραγωγής"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Λίστα  skins"
#define MSGTR_Network "Streaming δικτύου."
#define MSGTR_Preferences "Ιδιότητες"
#define MSGTR_NoMediaOpened "Δεν φορτώθηκαν αρχεία"
#define MSGTR_VCDTrack "VCD track %d"
#define MSGTR_NoChapter "Μη χρήση κεφαλαίου"
#define MSGTR_Chapter "Κεφάλαιο %d"
#define MSGTR_NoFileLoaded "δεν φορτώθηκε αρχείο"

// --- buttons ---
#define MSGTR_Ok "Εντάξει"
#define MSGTR_Cancel "Άκυρο"
#define MSGTR_Add "Προσθήκη"
#define MSGTR_Remove "Αφαίρεση"
#define MSGTR_Clear "Καθάρισμα"
#define MSGTR_Config "Προτιμήσεις"
#define MSGTR_ConfigDriver "Προτίμηση οδηγού"
#define MSGTR_Browse "Αναζήτηση αρχείου"

// --- error messages ---
#define MSGTR_NEMDB "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για εγγραφή  στον buffer."
#define MSGTR_NEMFMR "Λυπάμαι, δεν υπάρχει αρκετή μνήμη για την εμφάνιση του μενού."
#define MSGTR_IDFGCVD "Λυπάμαι, δεν βρέθηκε οδηγός εξόδου βίντεο που να είναι συμβατός με το GUI."
#define MSGTR_NEEDLAVC "Λυπάμαι, δεν μπορείτε να αναπαράγετε αρχεία που δεν είναι mpeg με τη συσκευή DXR3/H+ χωρίς επανακωδικοποίηση.\n Παρακαλώ ενεργοποιήστε lavc στο DXR3/H+ κουτί-διαλόγου."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] σφάλμα στο αρχείο προτιμήσεων του skin στη γραμμή %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] προειδοποίηση στο αρχείο προτιμήσεων του skin στη γραμμή %d: το widget βρέθηκε αλλά πριν το \"section\" δεν βρέθηκε (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] προειδοποίηση στο αρχείο προτιμήσεων του skin στη γραμμή %d: το widget βρέθηκε αλλά πριν το \"subsection\" δεν βρέθηκε (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] προειδοποίηση στο αρχείο προτιμήσεων του skin στη γραμμή %d: αυτό το subsection δεν υποστηρίζεται από αυτό το widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "το βάθος χρώματος εικόνας των 16 bits ή λιγότερο δεν υποστηρίζεται (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "το αρχείο (%s) δεν βρέθηκε\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "σφάλμα κατά την ανάγνωση του BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "σφάλμα κατά την ανάγνωση του TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "σφάλμα κατά την ανάγνωση του PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "Το RLE packed TGA δεν υποστηρίζεται (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "μη αναγνωρίσιμο είδος αρχείου (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "σφάλμα κατά τη μετατροπή από 24 bit σε 32 bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "μη αναγνωρίσιμο μήνυμα: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "δεν υπάρχει αρκετή μνήμη διαθέσιμη\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "πολλαπλές ορισμένες γραμματοσειρές\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "δεν βρέθηκε αρχείο γραμματοσειράς\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "δεν βρέθηκε εικόνα του αρχείου γραμματοσειράς\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "μη-υπαρκτή η ταυτότητα της γραμματοσειράς (%s)\n"
#define MSGTR_SKIN_UnknownParameter "μη αναγνωρίσιμη παράμετρος (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Δεν βρέθηκε skin (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "σφάλμα ανάγνωσης του skin configfile (%s).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Περί του MPlayer"
#define MSGTR_MENU_Open "Άνοιγμα..."
#define MSGTR_MENU_PlayFile "Αναπαραγωγή αρχείου..."
#define MSGTR_MENU_PlayVCD "Αναπαραγωγή VCD..."
#define MSGTR_MENU_PlayDVD "Αναπαραγωγή DVD..."
#define MSGTR_MENU_PlayURL "Αναπαραγωγή URL..."
#define MSGTR_MENU_LoadSubtitle "Άνοιγμα υποτίτλου..."
#define MSGTR_MENU_DropSubtitle "Αφαίρεση υποτίτλου..."
#define MSGTR_MENU_LoadExternAudioFile "Άνοιγμα εξωτερικού αρχείου ήχου..."
#define MSGTR_MENU_Playing "Αναπαραγωγή..."
#define MSGTR_MENU_Play "Αναπαραγωγή"
#define MSGTR_MENU_Pause "Παύση"
#define MSGTR_MENU_Stop "Σταμάτημα"
#define MSGTR_MENU_NextStream "Επόμενο κανάλι"
#define MSGTR_MENU_PrevStream "Προηγούμενο κανάλι"
#define MSGTR_MENU_Size "Μέγεθος"
#define MSGTR_MENU_NormalSize "Κανονικό μέγεθος"
#define MSGTR_MENU_DoubleSize "Διπλάσιο μέγεθος"
#define MSGTR_MENU_FullScreen "Πλήρης οθόνη"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Αναπαραγωγή δίσκου..."
#define MSGTR_MENU_ShowDVDMenu "Εμφάνιση του μενού του DVD"
#define MSGTR_MENU_Titles "Τίτλοι"
#define MSGTR_MENU_Title "Τίτλος %2d"
#define MSGTR_MENU_None "(τίποτα)"
#define MSGTR_MENU_Chapters "Κεφάλαια"
#define MSGTR_MENU_Chapter "Κεφάλαιο %2d"
#define MSGTR_MENU_AudioLanguages "Γλώσσες ήχου"
#define MSGTR_MENU_SubtitleLanguages "Γλώσσες υποτίτλων"
#define MSGTR_MENU_SkinBrowser "Λίστα skins"
// TODO: Why is this different from MSGTR_Preferences?
#define MSGTR_MENU_Preferences "Ρυθμίσεις"
#define MSGTR_MENU_Exit "Έξοδος..."
#define MSGTR_MENU_Mute "Απενεργοποίηση ήχου"
#define MSGTR_MENU_Original "Αρχικό"
#define MSGTR_MENU_AspectRatio "Αναλογία εμφάνισης"
#define MSGTR_MENU_AudioTrack "Track ήχου"
#define MSGTR_MENU_Track "Track %d"
#define MSGTR_MENU_VideoTrack "Track βίντεο"

// --- equalizer
#define MSGTR_EQU_Audio "Ήχος"
#define MSGTR_EQU_Video "Βίντεο"
#define MSGTR_EQU_Contrast "Αντίθεση: "
#define MSGTR_EQU_Brightness "Φωτεινότητα: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturation: "
#define MSGTR_EQU_Front_Left "Μπροστά Αριστερά"
#define MSGTR_EQU_Front_Right "Μπροστά Δεξιά"
#define MSGTR_EQU_Back_Left "Πίσω αριστερά"
#define MSGTR_EQU_Back_Right "Πίσω δεξιά"
#define MSGTR_EQU_Center "Κέντρο"
#define MSGTR_EQU_Bass "Μπάσο"
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
#define MSGTR_PREFERENCES_SubtitleOSD "Υπότιτλοι και OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs και demuxer"
#define MSGTR_PREFERENCES_Misc "Διάφορα"

#define MSGTR_PREFERENCES_None "Τίποτα"
#define MSGTR_PREFERENCES_AvailableDrivers "Διαθέσιμοι οδηγοί:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Μη-αναπαραγωγή ήχου"
#define MSGTR_PREFERENCES_NormalizeSound "Κανονικοποίηση ήχου"
#define MSGTR_PREFERENCES_EnableEqualizer "Ενεργοποίηση του equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Ενεργοποίηση του extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficient:"
#define MSGTR_PREFERENCES_AudioDelay "Καθυστέρηση ήχου"
#define MSGTR_PREFERENCES_DoubleBuffer "Ενεργοποίηση double buffering"
#define MSGTR_PREFERENCES_DirectRender "Ενεργοποίηση direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Ενεργοποίηση κατάργησης καρέ"
#define MSGTR_PREFERENCES_HFrameDrop "Ενεργοποίηση σκληρής κατάργησης καρέ (επικίνδυνο)"
#define MSGTR_PREFERENCES_Flip "Flip της εικόνας πάνω-κάτω"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Μετρητής χρόνου και δείκτες"
#define MSGTR_PREFERENCES_OSDProgress "Μόνο Μπάρες Προόδου"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Χρόνος, ποσοστό επί της εκατό και συνολικός χρόνος"
#define MSGTR_PREFERENCES_Subtitle "Υπότιτλοι:"
#define MSGTR_PREFERENCES_SUB_Delay "Καθυστέρηση:"
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Θέση:"
#define MSGTR_PREFERENCES_SUB_AutoLoad "Απενεργοποίηση αυτόματου φορτώματος υποτίτλων"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode υπότιτλος"
#define MSGTR_PREFERENCES_SUB_MPSUB "Μετατροπή εισαγόμενου υπότιτλου σε υπότιτλο τύπου MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Μετατροπή εισαγόμενου υπότιτλου σε υπότιτλο τύπο SubViewer(SRT)"
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
#define MSGTR_PREFERENCES_Message "Προσοχή, μερικές λειτουργίες χρειάζονται επανεκκίνιση αναπαραγωγής."
#define MSGTR_PREFERENCES_DXR3_VENC "Κωδικοποιητής βίντεο:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Χρήση του LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Δυτικές Ευρωπαϊκές γλώσσες (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Δυτικές Ευρωπαϊκές γλώσσες με Ευρώ (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slavic/Central European Languages (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galician, Maltese, Τούρκικα (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Παλιά Βαλτική κωδικοσειρά (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Κυριλλική (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Αραβικά (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Νέα Ελληνικά (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Τούρκικα (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Βαλτική (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Κέλτικα (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Εβραϊκά (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Ρώσικα (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainian, Belarusian (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Απλοποιημένα Κινέζικα (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Παραδοσιακά Κινέζικα (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Γιαπωνέζικα (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Κορεάτικα (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Ταϊλανδέζικη κωδικοσειρά (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Κυριλλική Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Σλάβικα/Κεντρικά Ευρωπαϊκά Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Χωρίς αυτόματη κλιμάκωση"
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
#define MSGTR_PREFERENCES_LoadFullscreen "Εκκίνηση σε πλήρη οθόνη"
#define MSGTR_PREFERENCES_SaveWinPos "Αποθήκευση της θέσης του παραθύρου"
#define MSGTR_PREFERENCES_XSCREENSAVER "Απενεργοποίηση της προστασίας οθόνης"
#define MSGTR_PREFERENCES_PlayBar "Ενεργοποίηση της playbar"
#define MSGTR_PREFERENCES_AutoSync "Ενεργοποίηση/απενεργοποίηση του αυτόματου συγχρονισμού"
#define MSGTR_PREFERENCES_AutoSyncValue "Αυτόματος συγχρονισμός: "
#define MSGTR_PREFERENCES_CDROMDevice "Συσκευή CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Συσκευή DVD:"
#define MSGTR_PREFERENCES_FPS "FPS ταινίας:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Εμφάνιση του Video Window όταν δεν είναι ενεργοποιημένο"

#define MSGTR_ABOUT_UHU "Η ανάπτυξη του GUI προωθείται από την UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Κρίσιμο σφάλμα..."
#define MSGTR_MSGBOX_LABEL_Error "Σφάλμα..."
#define MSGTR_MSGBOX_LABEL_Warning "Προειδοποίηση..."

#endif
