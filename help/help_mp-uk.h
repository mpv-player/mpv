/* Translated by:  Volodymyr M. Lisivka <lvm@mystery.lviv.net>,
		   Andriy Gritsenko <andrej@lucky.net>
   Was synced with help_mp-en.h: rev 1.105
 ========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Запуск:   mplayer [опц╕╖] [path/]filename\n"
"\n"
"Опц╕╖:\n"
" -vo <drv[:dev]> виб╕р драйвера ╕ пристрою в╕део виводу (список див. з '-vo help')\n"
" -ao <drv[:dev]> виб╕р драйвера ╕ пристрою ауд╕о виводу (список див. з '-ao help')\n"
#ifdef HAVE_VCD
" vcd://<номер треку> грати VCD (video cd) трек з пристрою зам╕сть файлу\n"
#endif
#ifdef USE_DVDREAD
" dvd://<номер титр╕в> грати DVD титри/трек з пристрою зам╕сть файлу\n"
" -alang/-slang   вибрати мову DVD ауд╕о/субтитр╕в (двосимвольний код кра╖ни)\n"
#endif
" -ss <час>       перем╕ститися на задану (секунди або ГГ:ХХ:СС) позиц╕ю\n"
" -nosound        без звуку\n"
" -stereo <режим> виб╕р MPEG1 стерео виводу (0:стерео 1:л╕вий 2:правий)\n"
" -channels <n>   номер вих╕дних канал╕в звуку\n"
" -fs -vm -zoom   повноекранне програвання (повноекр.,зм╕на в╕део,масштабування\n"
" -x <x> -y <y>   маштабувати картинку до <x> * <y> [якщо -vo драйвер п╕дтриму╓!]\n"
" -sub <file>     вказати файл субтитр╕в (див. також -subfps, -subdelay)\n"
" -playlist <file> вказати playlist\n"
" -vid x -aid y   опц╕╖ для вибору в╕део (x) ╕ ауд╕о (y) потоку для програвання\n"
" -fps x -srate y опц╕╖ для зм╕ни в╕део (x кадр/сек) ╕ ауд╕о (y Hz) швидкост╕\n"
" -pp <quality>   дозволити ф╕льтр (0-4 для DivX, 0-63 для mpegs)\n"
" -nobps          використовувати альтернативний метод синхрон╕зац╕╖ A-V для AVI файл╕в (може допомогти!)\n"
" -framedrop      дозволити втрату кадр╕в (для пов╕льних машин)\n"
" -wid <id в╕кна> використовувати ╕снуюче в╕кно для в╕део виводу (корисно для plugger!)\n"
"\n"
"Клав╕ш╕:\n"
" <-  або ->      перемотування вперед/назад на 10 секунд\n"
" вверх або вниз  перемотування вперед/назад на  1 хвилину\n"
" pgup або pgdown перемотування вперед/назад на 10 хвилин\n"
" < або >         перемотування вперед/назад у списку програвання\n"
" p або ПРОБ╤Л    зупинити ф╕льм (будь-яка клав╕ша - продовжити)\n"
" q або ESC       зупинити в╕дтворення ╕ вих╕д\n"
" + або -         регулювати затримку звуку по +/- 0.1 секунд╕\n"
" o               цикл╕чний переб╕р OSD режим╕в:  нема / нав╕гац╕я / нав╕гац╕я+таймер\n"
" * або /         додати або зменшити гучн╕сть (натискання 'm' вибира╓ master/pcm)\n"
" z або x         регулювати затримку субтитр╕в по +/- 0.1 секунд╕\n"
" r or t          зм╕нити положення субтитр╕в вгору/вниз, також див. -vf expand\n"
"\n"
" * * * ДЕТАЛЬН╤ШЕ ДИВ. ДОКУМЕНТАЦ╤Ю, ПРО ДОДАТКОВ╤ ОПЦ╤╥ ╤ КЛЮЧ╤! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nВиходимо...\n"
#define MSGTR_ExitingHow "\nВиходимо... (%s)\n"
#define MSGTR_Exit_quit "Вих╕д"
#define MSGTR_Exit_eof "К╕нець файлу"
#define MSGTR_Exit_error "Фатальна помилка"
#define MSGTR_IntBySignal "\nMPlayer перерваний сигналом %d у модул╕: %s \n"
#define MSGTR_NoHomeDir "Не можу знайти домашн╕й каталог\n"
#define MSGTR_GetpathProblem "проблеми у get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Створення файлу конф╕гурац╕╖: %s\n"
#define MSGTR_InvalidAOdriver "Неприпустиме ╕м'я драйверу ауд╕о виводу: %s\nДив. '-ao help' щоб отримати список доступних драйвер╕в.\n"
#define MSGTR_CopyCodecsConf "(скоп╕юйте etc/codecs.conf (з текст╕в MPlayer) у ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Використовую вбудований codecs.conf\n"
#define MSGTR_CantLoadFont "Не можу завантажити шрифт: %s\n"
#define MSGTR_CantLoadSub "Не можу завантажити субтитри: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: обраний пот╕к загублений!\n"
#define MSGTR_CantOpenDumpfile "Не можу в╕дкрити файл дампу!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "К╕льк╕сть кадр╕в на секунду не вказано (або неприпустиме значення) у заголовку! Використовуйте ключ -fps!\n"
#define MSGTR_TryForceAudioFmt "Спроба примусово використати с╕мейство ауд╕о кодек╕в %d...\n"
#define MSGTR_CantFindAudioCodec "Не можу знайти кодек для ауд╕о формату 0x%X!\n"
#define MSGTR_TryForceVideoFmt "Спроба примусово використати с╕мейство в╕део кодек╕в %d...\n"
#define MSGTR_CantFindVideoCodec "Не можу знайти кодек для в╕део формату 0x%X!\n"
#define MSGTR_VOincompCodec "Вибачте, обраний video_out пристр╕й не сум╕сний з цим кодеком.\n"
#define MSGTR_CannotInitVO "FATAL: Не можу ╕н╕ц╕ал╕зувати в╕део драйвер!\n"
#define MSGTR_CannotInitAO "не можу в╕дкрити/╕н╕ц╕ал╕зувати ауд╕о пристр╕й -> ГРАЮ БЕЗ ЗВУКУ\n"
#define MSGTR_StartPlaying "Початок програвання...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ********************************************************\n"\
"         **** Ваша система надто ПОВ╤ЛЬНА щоб в╕дтворити це! ****\n"\
"         ********************************************************\n"\
"!!! Можлив╕ причини, проблеми, обх╕дн╕ шляхи: \n"\
"- Найб╕льш загальн╕: поганий/сирий _ауд╕о_ драйвер :\n"\
"  - спробуйте -ao sdl або використовуйте ALSA 0.5 або емуляц╕ю oss на ALSA 0.9.\n"\
"  - Experiment with different values for -autosync, 30 is a good start.\n"\
"- Пов╕льний в╕део вив╕д.\n"\
"  - спробуйте ╕нший -vo драйвер (список: -vo help) або спробуйте з -framedrop!\n"\
"- Пов╕льний ЦП. Не намагайтеся в╕дтворювати велик╕ dvd/divx на пов╕льних\n"\
"  процесорах! спробуйте -hardframedrop\n"\
"- Битий файл. Спробуйте р╕зн╕ комб╕нац╕╖: -nobps  -ni  -mc 0  -forceidx\n"\
"- Пов╕льний нос╕й (диски NFS/SMB, DVD, VCD та ╕н.). Спробуйте -cache 8192.\n"\
"- Ви використову╓те -cache для програвання неперемеженого AVI файлу?\n"\
"  - спробуйте -nocache.\n"\
"Читайте поради в файлах DOCS/HTML/en/video.html .\n"\
"Якщо н╕чого не допомогло, тод╕ читайте DOCS/HTML/en/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer був скомп╕льований БЕЗ п╕дтримки GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI вимага╓ X11!\n"
#define MSGTR_Playing "Програвання %s\n"
#define MSGTR_NoSound "Ауд╕о: без звуку!!!\n"
#define MSGTR_FPSforced "Примусово зм╕нена к╕льк╕сть кадр╕в на секунду на %5.3f (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Скомп╕львано з автовизначенням CPU - УВАГА - це не оптимально!\nДля отримання кращих результат╕в перекомп╕люйте MPlayer з --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Скомп╕льовано для x86 CPU з розширеннями:"
#define MSGTR_AvailableVideoOutputDrivers "Доступн╕ модул╕ в╕део виводу:\n"
#define MSGTR_AvailableAudioOutputDrivers "Доступн╕ модул╕ ауд╕о виводу:\n"
#define MSGTR_AvailableAudioCodecs "Доступн╕ ауд╕о кодеки:\n"
#define MSGTR_AvailableVideoCodecs "Доступн╕ в╕део кодеки:\n"
#define MSGTR_AvailableAudioFm "\nДоступн╕ (вбудован╕) групи/драйвера ауд╕о кодек╕в:\n"
#define MSGTR_AvailableVideoFm "\nДоступн╕ (вбудован╕) групи/драйвера в╕део кодек╕в:\n"
#define MSGTR_AvailableFsType "Доступн╕ вар╕анти повноекранного в╕деорежиму:\n"
#define MSGTR_UsingRTCTiming "Використовую апаратний таймер RTC (%ldГц).\n"
#define MSGTR_CannotReadVideoProperties "В╕део: Неможливо отримати властивост╕.\n"
#define MSGTR_NoStreamFound "Пот╕к не знайдено.\n"
#define MSGTR_ErrorInitializingVODevice "Помилка в╕дкриття/╕н╕ц╕ал╕зац╕╖ вибраного video_out (-vo) пристрою.\n"
#define MSGTR_ForcedVideoCodec "Примусовий в╕део кодек: %s\n"
#define MSGTR_ForcedAudioCodec "Примусовий ауд╕о кодек: %s\n"
#define MSGTR_Video_NoVideo "В╕део: без в╕део\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Неможливо ╕н╕ц╕ал╕зувати в╕део ф╕льтри (-vf) або в╕део вив╕д (-vo).\n"
#define MSGTR_Paused "\n  =====  ПАУЗА  =====\r"
#define MSGTR_PlaylistLoadUnable "\nНеможливо завантажити playlist %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer crashed by an 'Illegal Instruction'.\n"\
"  It may be a bug in our new runtime CPU-detection code...\n"\
"  Please read DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer crashed by an 'Illegal Instruction'.\n"\
"  It usually happens when you run it on a CPU different than the one it was\n"\
"  compiled/optimized for.\n  Verify this!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer crashed by bad usage of CPU/FPU/RAM.\n"\
"  Recompile MPlayer with --enable-debug and make a 'gdb' backtrace and\n"\
"  disassembly. For details, see DOCS/HTML/en/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer crashed. This shouldn't happen.\n"\
"  It can be a bug in the MPlayer code _or_ in your drivers _or_ in your\n"\
"  gcc version. If you think it's MPlayer's fault, please read\n"\
"  DOCS/HTML/en/bugreports.html and follow the instructions there. We can't and\n"\
"  won't help unless you provide this information when reporting a possible bug.\n"

// mencoder.c:
#define MSGTR_UsingPass3ControllFile "Використовую pass3 файл: %s\n"
#define MSGTR_MissingFilename "\nНевизначений файл.\n\n"
#define MSGTR_CannotOpenFile_Device "Неможливо в╕дкрити файл/пристр╕й.\n"
#define MSGTR_CannotOpenDemuxer "Неможливо в╕дкрити demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nНе вибраний ауд╕о кодек (-oac). Вибер╕ть або використовуйте -nosound. Спробуйте -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\nНе вибраний в╕део кодек (-ovc). Вибер╕ть, спробуйте -ovc help!\n"
#define MSGTR_CannotOpenOutputFile "Неможливо створити файл '%s'.\n"
#define MSGTR_EncoderOpenFailed "Неможливо в╕дкрити кодек.\n"
#define MSGTR_ForcingOutputFourcc "Встановлюю вих╕дний fourcc в %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Записую заголовок...\n"
#define MSGTR_DuplicateFrames "\n%d повторних кадр╕в!\n"
#define MSGTR_SkipFrame "\nКадр пропущено!\n"
#define MSGTR_ErrorWritingFile "%s: Помилка запису файлу.\n"
#define MSGTR_WritingAVIIndex "\nЗаписую index...\n"
#define MSGTR_FixupAVIHeader "Оновлюю заголовок...\n"
#define MSGTR_RecommendedVideoBitrate "Рекомендований б╕трейт для %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nВ╕део пот╕к: %8.3f kbit/s  (%d Б/с)  розм╕р: %d байт  %5.3f секунд  %d кадр╕в\n"
#define MSGTR_AudioStreamResult "\nАуд╕о пот╕к: %8.3f kbit/s  (%d Б/с)  розм╕р: %d байт  %5.3f секунд\n"

// cfg-mencoder.h:
#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     метод зм╕нного б╕трейту\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           приблизний б╕трейт\n"\
"\n"\
" cbr           пост╕йний б╕трейт\n"\
"               Forces also CBR mode encoding on subsequent ABR presets modes\n"\
"\n"\
" br=<0-1024>   вказати б╕трейт в kBit (т╕льки для CBR та ABR)\n"\
"\n"\
" q=<0-9>       як╕сть (0-найвища, 9-найнижча) (т╕льки для VBR)\n"\
"\n"\
" aq=<0-9>      алгор╕тмична як╕сть (0-краща/пов╕льн╕ша 9-г╕рша/швидк╕ша)\n"\
"\n"\
" ratio=<1-100> ступень стиснення\n"\
"\n"\
" vol=<0-10>    set audio input gain\n"\
"\n"\
" mode=<0-3>    (якщо не вказано: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: no\n"\
"                1: all\n"\
"                2: adjust\n"\
"\n"\
" fast          переходити на швидке кодування при посл╕довних VBR presets modes,\n"\
"               трохи менша як╕сть та б╕льш╕ б╕трейти.\n"\
"\n"\
" preset=<value> запровадити найб╕льш╕ установки якост╕.\n"\
"                 середня:    VBR кодування, добра як╕сть\n"\
"                 (150-180 kbps б╕трейт)\n"\
"                 стандарт:   VBR кодування, висока як╕сть\n"\
"                 (170-210 kbps б╕трейт)\n"\
"                 висока:     VBR кодування, дуже висока як╕сть\n"\
"                 (200-240 kbps б╕трейт)\n"\
"                 божев╕льна: CBR кодування, найвища настройка якост╕\n"\
"                 (320 kbps б╕трейт)\n"\
"                 <8-320>:    ABR кодування з вказаним приблизним б╕трейтом.\n\n"
   
// open.c, stream.c:
#define MSGTR_CdDevNotfound "Компактов╕д \"%s\" не знайдений!\n"
#define MSGTR_ErrTrackSelect "Помилка вибору треку на VCD!"
#define MSGTR_ReadSTDIN "Читання з stdin...\n"
#define MSGTR_UnableOpenURL "Не можу в╕дкрити URL: %s\n"
#define MSGTR_ConnToServer "З'╓днання з сервером: %s\n"
#define MSGTR_FileNotFound "Файл не знайдений: '%s'\n"

#define MSGTR_SMBFileNotFound "Помилка в╕дкриття з мереж╕: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer не ма╓ вкомп╕льовано╖ п╕дтримки SMB\n"

#define MSGTR_CantOpenDVD "Не зм╕г в╕дкрити DVD: %s\n"
#define MSGTR_DVDwait "Читання структури диска, почекайте будь ласка...\n"
#define MSGTR_DVDnumTitles "╢ %d дор╕жок з титрами на цьому DVD.\n"
#define MSGTR_DVDinvalidTitle "Неприпустимий номер дор╕жки титр╕в на DVD: %d\n"
#define MSGTR_DVDnumChapters "╢ %d розд╕л╕в на ц╕й дор╕жц╕ з DVD титрами.\n"
#define MSGTR_DVDinvalidChapter "Неприпустимий номер DVD розд╕лу: %d\n"
#define MSGTR_DVDnumAngles "╢ %d кут╕в на ц╕й дор╕жц╕ з DVD титрами.\n"
#define MSGTR_DVDinvalidAngle "Неприпустимий номер DVD кута: %d\n"
#define MSGTR_DVDnoIFO "Не можу в╕дкрити IFO файл для DVD титр╕в %d.\n"
#define MSGTR_DVDnoVOBs "Не можу в╕дкрити титри VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD усп╕шно в╕дкритий!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Попередження! Заголовок ауд╕о потоку %d перевизначений!\n"
#define MSGTR_VideoStreamRedefined "Попередження! Заголовок в╕део потоку %d перевизначений!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Надто багато (%d, %d байт╕в) ауд╕о пакет╕в у буфер╕!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Надто багато (%d, %d байт╕в) в╕део пакет╕в у буфер╕!\n"
#define MSGTR_MaybeNI "(можливо ви програ╓те неперемежений пот╕к/файл або невдалий кодек)\n"
#define MSGTR_SwitchToNi "\nДетектовано погано перемежений AVI файл - переходжу в -ni режим...\n"
#define MSGTR_Detected_XXX_FileFormat "Знайдений %s формат файлу!\n"
#define MSGTR_DetectedAudiofile "Ауд╕о файл детектовано.\n"
#define MSGTR_NotSystemStream "Не в формат╕ MPEG System Stream... (можливо, Transport Stream?)\n"
#define MSGTR_FormatNotRecognized "========= Вибачте, формат цього файлу не розп╕знаний чи не п╕дтриму╓ться ===========\n"\
				  "===== Якщо це AVI, ASF або MPEG пот╕к, будь ласка зв'яж╕ться з автором! ======\n"
#define MSGTR_MissingVideoStream "В╕део пот╕к не знайдений!\n"
#define MSGTR_MissingAudioStream "Ауд╕о пот╕к не знайдений...  -> програю без звуку\n"
#define MSGTR_MissingVideoStreamBug "В╕део пот╕к загублений!? Зв'яж╕ться з автором, це мабуть помилка :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: файл не м╕стить обраний ауд╕о або в╕део пот╕к\n"

#define MSGTR_NI_Forced "Примусово вибраний"
#define MSGTR_NI_Detected "Знайдений"
#define MSGTR_NI_Message "%s НЕПЕРЕМЕЖЕНИЙ формат AVI файлу!\n"

#define MSGTR_UsingNINI "Використання НЕПЕРЕМЕЖЕНОГО або пошкодженого формату AVI файлу!\n"
#define MSGTR_CouldntDetFNo "Не зм╕г визначити число кадр╕в (для абсолютного перенесення)\n"
#define MSGTR_CantSeekRawAVI "Не можу перем╕ститися у непро╕ндексованому потоц╕ .AVI! (вимага╓ться ╕ндекс, спробуйте з ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не можу перем╕щуватися у цьому файл╕!\n"

#define MSGTR_MOVcomprhdr "MOV: Стиснут╕ заголовки (поки що) не п╕дтримуються!\n"
#define MSGTR_MOVvariableFourCC "MOV: Попередження! Знайдено перем╕нний FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Попередження! надто багато трек╕в!"
#define MSGTR_FoundAudioStream "==> Знайдено ауд╕о пот╕к: %d\n"
#define MSGTR_FoundVideoStream "==> Знайдено в╕део пот╕к: %d\n"
#define MSGTR_DetectedTV "Детектовано ТВ! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Неможливо в╕дкрити ogg demuxer.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Пошук ауд╕о потоку (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Неможливо в╕дкрити ауд╕о пот╕к: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Неможливо в╕дкрити пот╕к субтитр╕в: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Не вдалося в╕дкрити ауд╕о demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Не вдалося в╕дкрити demuxer субтитр╕в: %s\n"
#define MSGTR_TVInputNotSeekable "TV input is not seekable! (Seeking will probably be for changing channels ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "╤нформац╕я демуксера %s вже присутня!\n"
#define MSGTR_ClipInfo "╤нформац╕я кл╕пу:\n"


// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не зм╕г в╕дкрити кодек\n"
#define MSGTR_CantCloseCodec "Не зм╕г закрити кодек\n"

#define MSGTR_MissingDLLcodec "ПОМИЛКА: Не зм╕г в╕дкрити необх╕дний DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не зм╕г завантажити чи ╕н╕ц╕ал╕зувати Win32/ACM AUDIO кодек (загублений DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не можу знайти кодек \"%s\" у libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: К╤НЕЦЬ ФАЙЛУ при пошуку посл╕довност╕ заголовк╕в\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не можу читати посл╕довн╕сть заголовк╕в!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читати розширення посл╕довност╕ заголовк╕в!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Погана посл╕довн╕сть заголовк╕в!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Погане розширення посл╕довност╕ заголовк╕в!\n"

#define MSGTR_ShMemAllocFail "Не можу захопити загальну пам'ять\n"
#define MSGTR_CantAllocAudioBuf "Не можу захопити вих╕дний буфер ауд╕о\n"

#define MSGTR_UnknownAudio "Нев╕домий чи загублений ауд╕о формат, програю без звуку\n"

#define MSGTR_UsingExternalPP "[PP] Використовую зовн╕шн╕й ф╕льтр обробки, макс q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Використовую обробку кодека, макс q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "В╕део атрибут '%s' не п╕дтриму╓ться вибраними vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Запрошений драйвер в╕део кодеку [%s] (vfm=%s) недосяжний (вв╕мкн╕ть його п╕д час комп╕ляц╕╖)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Запрошений драйвер ауд╕о кодеку [%s] (afm=%s) недосяжний (вв╕мкн╕ть його п╕д час комп╕ляц╕╖)\n"
#define MSGTR_OpeningVideoDecoder "В╕дкриваю в╕део декодер: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "В╕дкриваю ауд╕о декодер: [%s] %s\n"
#define MSGTR_UninitVideoStr "в╕дновлення в╕део: %s\n"
#define MSGTR_UninitAudioStr "в╕дновлення ауд╕о: %s\n"
#define MSGTR_VDecoderInitFailed "Зб╕й ╕н╕ц╕ал╕зац╕╖ VDecoder :(\n"
#define MSGTR_ADecoderInitFailed "Зб╕й ╕н╕ц╕ал╕зац╕╖ ADecoder :(\n"
#define MSGTR_ADecoderPreinitFailed "Зб╕й п╕дготування ADecoder :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Розпод╕ляю %d байт вх╕дному буферу\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Розпод╕ляю %d + %d = %d байт вих╕дному буферу\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Встановлення п╕дтримки lirc...\n"
#define MSGTR_LIRCdisabled "Ви не зможете використовувати ваше в╕ддалене керування\n"
#define MSGTR_LIRCopenfailed "Невдале в╕дкриття п╕дтримки lirc!\n"
#define MSGTR_LIRCcfgerr "Невдале читання файлу конф╕гурац╕╖ LIRC %s!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Неможливо знайти в╕део ф╕льтр '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Неможливо в╕дкрити в╕део ф╕льтр '%s'\n"
#define MSGTR_OpeningVideoFilter "В╕дкриваю в╕део ф╕льтр: "
//-----------------------------
#define MSGTR_CannotFindColorspace "Не можу п╕д╕брати загальну схему кольор╕в, нав╕ть додавши 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Кодек не встановив sh->disp_w та sh->disp_h, спробую об╕йти це.\n"
#define MSGTR_VoConfigRequest "VDec: vo config запит - %d x %d (preferred csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Не можу п╕д╕брати п╕дходящу схему кольор╕в - повтор з -vf scale...\n"
#define MSGTR_MovieAspectIsSet "В╕дношення стор╕н %.2f:1 - масштабую аби скоректувати.\n"
#define MSGTR_MovieAspectUndefined "В╕дношення стор╕н не вказано - масштабування не використову╓ться.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Про програму"
#define MSGTR_FileSelect "Вибрати файл..."
#define MSGTR_SubtitleSelect "Вибрати субтитри..."
#define MSGTR_OtherSelect "Виб╕р..."
#define MSGTR_AudioFileSelect "Вибрати зовн╕шн╕й ауд╕о канал..."
#define MSGTR_FontSelect "Вибрати шрифт..."
#define MSGTR_PlayList "Список програвання"
#define MSGTR_Equalizer "Еквалайзер"
#define MSGTR_SkinBrowser "Переглядач жупан╕в"
#define MSGTR_Network "Програвання з мереж╕..."
#define MSGTR_Preferences "Налаштування"
#define MSGTR_NoMediaOpened "Нема╓ в╕дкритого нос╕ю."
#define MSGTR_VCDTrack "Дор╕жка VCD %d"
#define MSGTR_NoChapter "No chapter"
#define MSGTR_Chapter "Chapter %d"
#define MSGTR_NoFileLoaded "Нема╓ завантаженого файлу."

// --- buttons ---
#define MSGTR_Ok "Так"
#define MSGTR_Cancel "Скасувати"
#define MSGTR_Add "Додати"
#define MSGTR_Remove "Видалити"
#define MSGTR_Clear "Вичистити"
#define MSGTR_Config "Конф╕гурувати"
#define MSGTR_ConfigDriver "Конф╕гурувати драйвер"
#define MSGTR_Browse "Проглядати"

// --- error messages ---
#define MSGTR_NEMDB "Вибачте, не вистача╓ пам'ят╕ для в╕дмальовування буферу."
#define MSGTR_NEMFMR "Вибачте, не вистача╓ пам'ят╕ для в╕дображення меню."
#define MSGTR_IDFGCVD "Вибачте, не знайдено в╕дпов╕дного до GUI вих╕дного в╕део драйверу."
#define MSGTR_NEEDLAVCFAME "Вибачте, ви не можете грати не-MPEG файли на вашому DXR3/H+ пристро╖ без перекодування.\nБудь ласка, вв╕мкн╕ть lavc або fame в панел╕ конф╕гурування DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[жупан] помилка у файл╕ конф╕гурац╕╖ жупана, рядок %d  : %s" 
#define MSGTR_SKIN_WARNING1 "[жупан] попередження: у файл╕ конф╕гурац╕╖ жупана, рядок %d: widget знайдений але до цього не знайдено \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[жупан] попередження: у файл╕ конф╕гурац╕╖ жупана, рядок %d: widget знайдений але до цього не знайдено \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[жупан] попередження: у файл╕ конф╕гурац╕╖ жупана, рядок %d: цей widget (%s) не п╕дтриму╓ цю subsection"
#define MSGTR_SKIN_BITMAP_16bit  "Глибина кольору б╕тово╖ карти у 16 б╕т ╕ менше не п╕дтриму╓ться (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "файл не знайдений (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "помилка читання BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "помилка читання TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "помилка читання PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE запакований TGA не п╕дтриму╓ться (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "нев╕домий тип файлу (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "помилка перетворення 24-б╕т у 32-б╕т (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "нев╕доме пов╕домлення: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "не вистача╓ пам'ят╕\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "оголошено надто багато шрифт╕в\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "файл шрифту не знайдений\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "файл образ╕в шрифту не знайдений\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "не╕снуючий ╕дентиф╕катор шрифту (%s)\n"
#define MSGTR_SKIN_UnknownParameter "нев╕домий параметр (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Жупан не знайдено (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Помилка читання файла конф╕гурац╕╖ жупана (%s).\n"
#define MSGTR_SKIN_LABEL "Жупани:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Про програму"
#define MSGTR_MENU_Open "В╕дкрити..."
#define MSGTR_MENU_PlayFile "Грати файл..."
#define MSGTR_MENU_PlayVCD "Грати VCD..."
#define MSGTR_MENU_PlayDVD "Грати DVD..."
#define MSGTR_MENU_PlayURL "Грати URL..."
#define MSGTR_MENU_LoadSubtitle "Завантажити субтитри..."
#define MSGTR_MENU_DropSubtitle "Викинути субтитри..."
#define MSGTR_MENU_LoadExternAudioFile "Завантажити зовн╕шн╕й ауд╕о файл..."
#define MSGTR_MENU_Playing "В╕дтворення"
#define MSGTR_MENU_Play "Грати"
#define MSGTR_MENU_Pause "Пауза"
#define MSGTR_MENU_Stop "Зупинити"
#define MSGTR_MENU_NextStream "Наступний пот╕к"
#define MSGTR_MENU_PrevStream "Попередн╕й пот╕к"
#define MSGTR_MENU_Size "Розм╕р"
#define MSGTR_MENU_NormalSize "Нормальний розм╕р"
#define MSGTR_MENU_DoubleSize "Подв╕йний розм╕р"
#define MSGTR_MENU_FullScreen "Повний екран"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Грати диск..."
#define MSGTR_MENU_ShowDVDMenu "Показати DVD меню"
#define MSGTR_MENU_Titles "Титри"
#define MSGTR_MENU_Title "Титр %2d"
#define MSGTR_MENU_None "(нема)"
#define MSGTR_MENU_Chapters "Розд╕ли"
#define MSGTR_MENU_Chapter "Розд╕л %2d"
#define MSGTR_MENU_AudioLanguages "Авто мова"
#define MSGTR_MENU_SubtitleLanguages "Мова субтитр╕в"
#define MSGTR_MENU_PlayList "Список програвання"
#define MSGTR_MENU_SkinBrowser "Переглядач жупан╕в"
#define MSGTR_MENU_Preferences "Налаштування"
#define MSGTR_MENU_Exit "Вих╕д..."
#define MSGTR_MENU_Mute "Тиша"
#define MSGTR_MENU_Original "Вих╕дний"
#define MSGTR_MENU_AspectRatio "В╕дношення стор╕н"
#define MSGTR_MENU_AudioTrack "Ауд╕о дор╕жка"
#define MSGTR_MENU_Track "Дор╕жка %d"
#define MSGTR_MENU_VideoTrack "В╕део дор╕жка"

// --- equalizer
#define MSGTR_EQU_Audio "Ауд╕о"
#define MSGTR_EQU_Video "В╕део"
#define MSGTR_EQU_Contrast "Контраст: "
#define MSGTR_EQU_Brightness "Яскрав╕сть: "
#define MSGTR_EQU_Hue "Тон: "
#define MSGTR_EQU_Saturation "Насичен╕сть: "
#define MSGTR_EQU_Front_Left "Передн╕й Л╕вий"
#define MSGTR_EQU_Front_Right "Передн╕й Правий"
#define MSGTR_EQU_Back_Left "Задн╕й Л╕вий"
#define MSGTR_EQU_Back_Right "Задн╕й Правий"
#define MSGTR_EQU_Center "Центральний"
#define MSGTR_EQU_Bass "Бас"
#define MSGTR_EQU_All "Ус╕"
#define MSGTR_EQU_Channel1 "Канал 1:"
#define MSGTR_EQU_Channel2 "Канал 2:"
#define MSGTR_EQU_Channel3 "Канал 3:"
#define MSGTR_EQU_Channel4 "Канал 4:"
#define MSGTR_EQU_Channel5 "Канал 5:"
#define MSGTR_EQU_Channel6 "Канал 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Шлях"
#define MSGTR_PLAYLIST_Selected "Вибран╕ файли"
#define MSGTR_PLAYLIST_Files "Файли"
#define MSGTR_PLAYLIST_DirectoryTree "Дерево каталогу"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Ауд╕о"
#define MSGTR_PREFERENCES_Video "В╕део"
#define MSGTR_PREFERENCES_SubtitleOSD "Субтитри й OSD"
#define MSGTR_PREFERENCES_Codecs "Кодеки й demuxer"
#define MSGTR_PREFERENCES_Misc "Р╕зне"

#define MSGTR_PREFERENCES_None "Нема╓"
#define MSGTR_PREFERENCES_AvailableDrivers "Доступн╕ драйвери:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Не грати звук"
#define MSGTR_PREFERENCES_NormalizeSound "Нормал╕зувати звук"
#define MSGTR_PREFERENCES_EnEqualizer "Дозволити еквалайзер"
#define MSGTR_PREFERENCES_ExtraStereo "Дозволити додаткове стерео"
#define MSGTR_PREFERENCES_Coefficient "Коеф╕ц╕ент:"
#define MSGTR_PREFERENCES_AudioDelay "Затримка ауд╕о"
#define MSGTR_PREFERENCES_DoubleBuffer "Дозволити подв╕йне буферування"
#define MSGTR_PREFERENCES_DirectRender "Дозволити прямий вив╕д"
#define MSGTR_PREFERENCES_FrameDrop "Дозволити пропуск кадр╕в"
#define MSGTR_PREFERENCES_HFrameDrop "Дозволити викидування кадр╕в (небезпечно)"
#define MSGTR_PREFERENCES_Flip "Перегорнути зображення догори ногами"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Таймер та ╕нд╕катори"
#define MSGTR_PREFERENCES_OSDProgress "Лише л╕н╕йки"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Таймер, проценти та загальний час"
#define MSGTR_PREFERENCES_Subtitle "Субтитри:"
#define MSGTR_PREFERENCES_SUB_Delay "Затримка: "
#define MSGTR_PREFERENCES_SUB_FPS "к/c:"
#define MSGTR_PREFERENCES_SUB_POS "Положення: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Заборонити автозавантаження субтитр╕в"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode субтитри"
#define MSGTR_PREFERENCES_SUB_MPSUB "Перетворити вказан╕ субтитри до формату MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Перетворити вказан╕ субтитри до формату SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Дозволити/заборонити перекриття субтитр╕в"
#define MSGTR_PREFERENCES_Font "Шрифт:"
#define MSGTR_PREFERENCES_FontFactor "Фактор шрифту:"
#define MSGTR_PREFERENCES_PostProcess "Дозволити postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Авто як╕сть: "
#define MSGTR_PREFERENCES_NI "Використовувати неперемежений AVI парсер"
#define MSGTR_PREFERENCES_IDX "Перебудувати ╕ндекс, якщо треба"
#define MSGTR_PREFERENCES_VideoCodecFamily "Драйвер в╕део содеку:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Драйвер ауд╕о кодеку:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Р╕вень OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Субтитри"
#define MSGTR_PREFERENCES_FRAME_Font "Шрифт"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Кодек й demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Кеш"
#define MSGTR_PREFERENCES_FRAME_Misc "Р╕зне"
#define MSGTR_PREFERENCES_Message "Не забудьте, що вам треба перезапустити програвання для набуття чинност╕ деяких параметр╕в!"
#define MSGTR_PREFERENCES_DXR3_VENC "В╕део кодек:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Використовувати LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Використовувати FAME"
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
#define MSGTR_PREFERENCES_FontNoAutoScale "Без автомасштабування"
#define MSGTR_PREFERENCES_FontPropWidth "Пропорц╕йно ширин╕ кадру"
#define MSGTR_PREFERENCES_FontPropHeight "Пропорц╕йно висот╕ кадру"
#define MSGTR_PREFERENCES_FontPropDiagonal "Пропорц╕йно д╕агонал╕ кадру"
#define MSGTR_PREFERENCES_FontEncoding "Кодування:"
#define MSGTR_PREFERENCES_FontBlur "Розпливання:"
#define MSGTR_PREFERENCES_FontOutLine "Обведення:"
#define MSGTR_PREFERENCES_FontTextScale "Масштаб тексту:"
#define MSGTR_PREFERENCES_FontOSDScale "Масштаб OSD:"
#define MSGTR_PREFERENCES_Cache "Кеш on/off"
#define MSGTR_PREFERENCES_CacheSize "Розм╕р кешу: "
#define MSGTR_PREFERENCES_LoadFullscreen "Стартувати в полний екран"
#define MSGTR_PREFERENCES_SaveWinPos "Збер╕гати положення в╕кна"
#define MSGTR_PREFERENCES_XSCREENSAVER "Stop XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Дозволити л╕н╕йку програвання"
#define MSGTR_PREFERENCES_AutoSync "AutoSync on/off"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM пристр╕й:"
#define MSGTR_PREFERENCES_DVDDevice "DVD пристр╕й:"
#define MSGTR_PREFERENCES_FPS "Кадр╕в на секунду:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Показувати неактивне в╕кно зображення"

#define MSGTR_ABOUT_UHU "GUI розробку спонсовано UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   MPlayer команда розробник╕в:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Додатков╕ кодувальники:\n"
#define MSGTR_ABOUT_MainTesters "   Головн╕ тестувач╕:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "фатальна помилка..."
#define MSGTR_MSGBOX_LABEL_Error "помилка..."
#define MSGTR_MSGBOX_LABEL_Warning "попередження..." 

#endif
