// Sync'ed with help_mp-en.h 1.167
//
// Преведено от А. Димитров, plazmus@gmail.com
// Всички предложения са добре дошли.

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Употреба:   mplayer [опции] [url|път/]име_на_файл\n"
"\n"
"Основни опции:   (пълният списък е в ръководството - 'man mplayer')\n"
" -vo <дрв[:устр]>  избор на видео драйвер & устройство ('-vo help' дава списък)\n"
" -ao <дрв[:устр]>  избор на звуков драйвер & устройство ('-ao help' дава списък)\n"
#ifdef CONFIG_VCD
" vcd://<пътечка>   пуска (S)VCD (Super Video CD) пътечка (без монтиране!)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<номер>     пуска DVD заглавие от устройство, вместо от файл\n"
" -alang/-slang     избор на език за DVD аудиo/субтитри (чрез 2-буквен код)\n"
#endif
" -ss <позиция>     превъртане до дадена (в секунди или чч:мм:сс) позиция\n"
" -nosound          изключване на звука\n"
" -fs               пълноекранно възпроизвеждане (или -vm, -zoom, вж. manpage)\n"
" -x <x> -y <y>     избор на резолюция (използва се с -vm или -zoom)\n"
" -sub <файл>       задава файла със субтитри (вижте също -subfps и -subdelay)\n"
" -playlist <файл>  отваря playlist файл\n"
" -vid x -aid y     избор на видео (x) и аудио (y) поток за възпроизвеждане\n"
" -fps x -srate y   смяна на видео (x кадри в секунда) и аудио (y Hz) честотата\n"
" -pp <качество>    включва филтър за допълнителна обработка на образа\n"
"                   вижте ръководството и документацията за подробности\n"
" -framedrop        разрешава прескачането на кадри (при бавни машини)\n"
"\n"
"Основни клавиши:   (пълен списък има в ръководството, проверете също input.conf)\n"
" <-  или  ->       превърта назад/напред с 10 секунди\n"
" up или down       превърта назад/напред с 1 минута\n"
" pgup или pgdown   превърта назад/напред с 10 минути\n"
" < или >           стъпка назад/напред в playlist списъка\n"
" p или SPACE       пауза (натиснете произволен клавиш за продължение)\n"
" q или ESC         спиране на възпроизвеждането и изход от програмата\n"
" + или -           промяна закъснението на звука с +/- 0.1 секунда\n"
" o                 превключва OSD режима: без/лента за превъртане/лента и таймер\n"
" * или /           увеличава или намалява силата на звука (PCM)\n"
" z или x           променя закъснението на субтитрите с +/- 0.1 секунда\n"
" r или t           премества субтитрите нагоре/надолу, вижте и -vf expand\n"
"\n"
" * * * ЗА ПОДРОБНОСТИ, ДОПЪЛНИТЕЛНИ ОПЦИИ И КЛАВИШИ, ВИЖТЕ РЪКОВОДСТВОТО! * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Мостри от този формат са нужни за подобряване на поддръжката. Свържете се с нас!\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nИзлизане от програмата...\n"
#define MSGTR_ExitingHow "\nИзлизане от програмата... (%s)\n"
#define MSGTR_Exit_quit "Изход"
#define MSGTR_Exit_eof "Край на файла"
#define MSGTR_Exit_error "Фатална грешка"
#define MSGTR_IntBySignal "\nMPlayer е прекъснат от сигнал %d в модул: %s\n"
#define MSGTR_NoHomeDir "HOME директорията не може да бъде открита.\n"
#define MSGTR_GetpathProblem "Проблем с функция get_path(\"config\") \n"
#define MSGTR_CreatingCfgFile "Създава се конфигурационен файл: %s\n"
#define MSGTR_BuiltinCodecsConf "Използва се вградения codecs.conf.\n"
#define MSGTR_CantLoadFont "Не може да се зареди шрифт: %s\n"
#define MSGTR_CantLoadSub "Не могат да бъдат заредени субтитри: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: ФАТАЛНО: Избраният поток липсва!\n"
#define MSGTR_CantOpenDumpfile "Не може да се отвори файл за извличане.\n"
#define MSGTR_CoreDumped "Данните извлечени ;)\n"
#define MSGTR_FPSnotspecified "Броя кадри в секунда не е указан или е невалиден, ползвайте опцията -fps .\n"
#define MSGTR_TryForceAudioFmtStr "Опит за ползване на фамилия аудио кодеци %s...\n"
#define MSGTR_CantFindAudioCodec "Не може да бъде намерен кодек за този аудио формат 0x%X.\n"
#define MSGTR_RTFMCodecs "Прочетете DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Опит за ползване на фамилия видео кодеци %s...\n"
#define MSGTR_CantFindVideoCodec "Няма подходящ кодек за указаните -vo и видео формат 0x%X.\n"
#define MSGTR_CannotInitVO "ФАТАЛНО: Видео драйвера не може да бъде инициализиран.\n"
#define MSGTR_CannotInitAO "Аудио устройството не може да бъде отворено/инициализирано -> няма звук.\n"
#define MSGTR_StartPlaying "Започва възпроизвеждането...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ************************************************\n"\
"           **** Вашата система е твърде БАВНА за това!  ****\n"\
"           ************************************************\n\n"\
"Възможни причини, проблеми, решения:\n"\
"- Най-вероятно: неработещ/бъгав _аудио_ драйвер\n"\
"  - Опитайте -ao sdl или ползвайте OSS емулацията на ALSA.\n"\
"  - Експериментирайте с различни стойности на -autosync, 30 е добро начало.\n"\
"- Бавно видео извеждане\n"\
"  - Опитайте друг -vo драйвер (-vo help за списък) или пробвайте -framedrop!\n"\
"- Бавен процесор\n"\
"  - Не пускайте голям DVD/DivX филм на бавен процесор! Пробвайте -hardframedrop.\n"\
"- Повреден файл\n"\
"  - Опитайте различни комбинации от  -nobps -ni -forceidx -mc 0.\n"\
"- Бавен източник (NFS/SMB, DVD, VCD и т.н.)\n"\
"  - Опитайте -cache 8192.\n"\
"- Използвате -cache за non-interleaved AVI файл?\n"\
"  - Опитайте -nocache.\n"\
"Прочетете DOCS/HTML/en/video.html за съвети относно настройките.\n"\
"Ако нищо не помага, прочетете DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer е компилиран без графичен интерфейс.\n"
#define MSGTR_GuiNeedsX "Графичния интерфейс на MPlayer изисква X11.\n"
#define MSGTR_Playing "Възпроизвеждане на %s.\n"
#define MSGTR_NoSound "Аудио: няма звук\n"
#define MSGTR_FPSforced "Наложени са %5.3f кадъра в секунда (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Компилиран с динамично установяване на процесора - ВНИМАНИЕ - това не е оптимално!\nЗа най-добра производителност, рекомпилирайте MPlayer с --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Компилиран за x86 процесори с разширения:"
#define MSGTR_AvailableVideoOutputDrivers "Достъпни видео драйвери:\n"
#define MSGTR_AvailableAudioOutputDrivers "Достъпни аудио драйвери:\n"
#define MSGTR_AvailableAudioCodecs "Достъпни аудио кодеци:\n"
#define MSGTR_AvailableVideoCodecs "Достъпни видео кодеци:\n"
#define MSGTR_AvailableAudioFm "Достъпни (вградени) фамилии аудио кодеци/драйвери:\n"
#define MSGTR_AvailableVideoFm "Достъпни (вградени) фамилии видео кодеци/драйвери:\n"
#define MSGTR_AvailableFsType "Достъпни пълноекранни режими:\n"
#define MSGTR_UsingRTCTiming "Използва се хардуерния RTC таймер (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Видео: Параметрите не могат да бъдат прочетени.\n"
#define MSGTR_NoStreamFound "Не е открит поток.\n"
#define MSGTR_ErrorInitializingVODevice "Грешка при отваряне/инициализиране на избраното видео устройство (-vo).\n"
#define MSGTR_ForcedVideoCodec "Наложен видео кодек: %s\n"
#define MSGTR_ForcedAudioCodec "Наложен аудио кодек: %s\n"
#define MSGTR_Video_NoVideo "Видео: няма видео\n"
#define MSGTR_NotInitializeVOPorVO "\nФАТАЛНО: Видео филтъра (-vf) или изхода (-vo) не могат да бъдат инициализирани.\n"
#define MSGTR_Paused "\n  =====  ПАУЗА  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nPlaylist-ът не може да бъде зареден %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer катастрофира заради 'Невалидна инструкция'.\n"\
"  Може да е бъг в кода за динамично установяване на процесора...\n"\
"  Моля прочетете DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer катастрофира заради 'Невалидна инструкция'.\n"\
"  Това обикновено се случва когато бъде пуснат на процесор, различен от този\n"\
"  за който е компилиран/оптимизиран.\n"\
"  Проверете това!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer катастрофира заради лоша употреба на процесора/копроцесора/паметта.\n"\
"  рекомпилирайте MPlayer с --enable-debug и направете  backtrace и\n"\
"  дизасемблиране с 'gdb'.\nЗа подробности - DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer катастрофира. Tова не трябва да се случва.\n"\
"  Може да е бъг в кода на MPlayer _или_ във драйверите ви _или_ във\n"\
"  версията на gcc. Ако смятате, че е по вина на MPlayer, прочетете\n"\
"  DOCS/HTML/en/bugreports.html и следвайте инструкциите там. Ние не можем\n"\
"  и няма да помогнем, ако не осигурите тази информация, когато съобщавате за бъг.\n"
#define MSGTR_LoadingConfig "Зарежда се конфигурационен файл '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: добавен е файл със субтитри (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Грешка при отваряне на файла [%s] за запис!\n"
#define MSGTR_CommandLine "Команден ред:"
#define MSGTR_RTCDeviceNotOpenable "Грешка при отваряне на %s: %s (необходими са права за четене).\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC грешка при инициализация в ioctl (rtc_irqp_set кд%lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Добавете \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" към системните стартови скриптове.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC init грешка в ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "използва се  %s таймер.\n"
#define MSGTR_MenuInitialized "Менюто е инициализирано: %s\n"
#define MSGTR_MenuInitFailed "Менюто не може да бъде инициализирано.\n"
#define MSGTR_Getch2InitializedTwice "Внимание: Функцията getch2_init е извикана двукратно!\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Видео филтъра libmenu не може да бъде отворен без root меню %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Грешка при предварителна инициализация на аудио филтрите!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC грешка при четене: %s\n"
#define MSGTR_SoftsleepUnderflow "Внимание! Softsleep underflow!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV Събитие NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV Събитие: Highlight event broken\n"
#define MSGTR_DvdnavEvent "DVDNAV Събитие: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV Събитие: Highlight Hide\n"
#define MSGTR_DvdnavStillFrame "###################################### DVDNAV Събитие: Неподвижен кадър: %d сек\n"
#define MSGTR_DvdnavNavStop "DVDNAV Събитие: Nav Стоп\n"
#define MSGTR_DvdnavNavNOP "DVDNAV Събитие: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV Събитие: Nav Смяна на SPU Поток: физ: %d/%d/%d лог: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV Събитие: Nav Смяна на SPU Поток: физ: %d лог: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV Събитие: Nav Смяна на Аудио Поток: физ: %d лог: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV Събитие: Nav Смяна на VTS\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV Събитие: Nav Смяна на Клетка\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV Събитие: Nav Смяна на SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV Събитие: Nav Превъртането Приключено\n"
#define MSGTR_MenuCall "Menu call\n"

#define MSGTR_EdlOutOfMem "Не може да се задели достатъчно памет за EDL данните.\n"
#define MSGTR_EdlRecordsNo "Прочетени са %d EDL действия.\n"
#define MSGTR_EdlQueueEmpty "Няма EDL действия, които да бъдат извършени.\n"
#define MSGTR_EdlCantOpenForWrite "EDL файла [%s] не може да бъде отворен за запис.\n"
#define MSGTR_EdlCantOpenForRead "EDL файла [%s] не може да бъде отворен за четене.\n"
#define MSGTR_EdlNOsh_video "EDL не може да се ползва без видео, изключва се.\n"
#define MSGTR_EdlNOValidLine "Невалиден ред в EDL: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Зле форматиран EDL ред [%d] Отхвърля се.\n"
#define MSGTR_EdlBadLineOverlap "Последната позиция за спиране беше [%f]; следващата за пускане е "\
"[%f]. Елементите в списъка трябва да са в хронологичен ред, не могат да се препокриват.\n"
#define MSGTR_EdlBadLineBadStop "Времето за спиране трябва да е след времето за пускане.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Използва се файл за контрол на pass3: %s\n"
#define MSGTR_MissingFilename "\nЛипсва име на файл.\n\n"
#define MSGTR_CannotOpenFile_Device "Файла/устройството не може да бъде отворен.\n"
#define MSGTR_CannotOpenDemuxer "Не може да бъде отворен разпределител.\n"
#define MSGTR_NoAudioEncoderSelected "\nНе е избран аудио енкодер (-oac). Изберете или енкодер (вижте -oac help) или опцията -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nНе е избран видео енкодер (-ovc). Изберете си (вижте -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Изходния файл '%s'не може да бъде отворен.\n"
#define MSGTR_EncoderOpenFailed "Енкодерът не може да бъде отворен.\n"
#define MSGTR_ForcingOutputFourcc "Налагане на изходния fourcc код да бъде %x [%.4s]\n"
#define MSGTR_DuplicateFrames "\n%d дублиращи се кадъра!\n"
#define MSGTR_SkipFrame "\nПрескочен кадър!\n"
#define MSGTR_ResolutionDoesntMatch "\nНовият видео файл има различна резолюция или цветови формат от предишния.\n" 
#define MSGTR_FrameCopyFileMismatch "\nВсички видео файлове трябва да имат идентични резолюции, кадрови честоти и кодеци за -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nВсички файлове трябва да имат идентични аудио кодеци и формати за -oac copy.\n"
#define MSGTR_NoSpeedWithFrameCopy "ПРЕДУПРЕЖДЕНИЕ: -speed не работи гарантирано правилно с -oac copy!\n"\
"Кодирането ви може да се окаже повредено!\n"
#define MSGTR_ErrorWritingFile "%s: Грешка при запис на файла.\n"
#define MSGTR_RecommendedVideoBitrate "Препоръчителен битрейт за %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nВидео поток: %8.3f Кбита/с  (%d B/s)  размер: %"PRIu64" байта  %5.3f сек.  %d кадъра\n"
#define MSGTR_AudioStreamResult "\nАудио поток: %8.3f Кбита/с  (%d B/s)  размер: %"PRIu64" байта  %5.3f сек.\n"
#define MSGTR_OpenedStream "успех: формат: %d  данни: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "Избрано е CBR (постоянен битрейт) PCM аудио\n"
#define MSGTR_MP3AudioSelected "Избрано е MP3 аудио\n"
#define MSGTR_CannotAllocateBytes "Не може да се заделят %d байта\n"
#define MSGTR_SettingAudioDelay "АУДИО ЗАКЪСНЕНИЕТО е настроено на %5.3f\n"
#define MSGTR_SettingAudioInputGain "Аудио усилването е нагласено на %f\n"
#define MSGTR_LamePresetEquals "\nпрофил=%s\n\n"
#define MSGTR_LimitingAudioPreload "Предварителното аудио зареждане е ограничено на 0.4с\n"
#define MSGTR_IncreasingAudioDensity "Гъстотата на звука е увеличена на 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Налагане на нулево предварително аудио зареждане, max pts correction to 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR аудио: %d байта/сек, %d байта за блок\n"
#define MSGTR_LameVersion "LAME версия %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Грешка: Указаният битрейт е извън допустимите граници за този профил\n"\
"\n"\
"Когато използвате този режим трябва да въведете стойност между \"8\" и \"320\"\n"\
"\n"\
"Допълнителна информация може да получите с: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Грешка: Не сте въвели валиден профил и/или опции с preset\n"\
"\n"\
"Достъпните профили са:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Режим) - Не е нужно изрично да указвате ABR режима.\n"\
"                      За да го ползвате, просто укажете битрейт. Например:\n"\
"                      \"preset=185\" активира този\n"\
"                      профил и ползва средно 185 килобита в секунда.\n"\
"\n"\
"    Няколко примера:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" или \"-lameopts  cbr:preset=192       \"\n"\
" или \"-lameopts      preset=172       \"\n"\
" или \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Допълнителна информация можете да получите с: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Профилите са създадени за да осигуряват най-доброто възможно качество.\n"\
"\n"\
"В по-голямата си част те са били предмет на сериозни тестове\n"\
"за да се осигури и потвърди това качество.\n"\
"\n"\
"Непрекъснато се обновяват, съгласно най-новите разработки\n"\
"и полученият резултат би трябвало да ви осигури най-доброто\n"\
"качество постижимо с LAME.\n"\
"\n"\
"За да активирате тези профили:\n"\
"\n"\
"   За VBR режими (най-високо качество):\n"\
"\n"\
"     \"preset=standard\" Tози профил е подходящ за повеето хора и повечето\n"\
"                             видове музика и притежава доста високо качество.\n"\
"\n"\
"     \"preset=extreme\" Ако имате изключително добър слух и оборудване от\n"\
"                             високо ниво, този профил ще осигури\n"\
"                             малко по-добро качество от \"standard\"\n"\
"                             режима.\n"\
"\n"\
"   За CBR 320Кбита/с (профил с най-високото възможно качество):\n"\
"\n"\
"     \"preset=insane\"  Настройките в този профил са прекалени за повечето\n"\
"                             хора и ситуации, но ако се налага\n"\
"                             да постигнете абсолютно максимално качество\n"\
"                             без значение от размера на файла, това е начина.\n"\
"\n"\
"   За ABR режим (високо качество при зададем битрейт, но не колкото при VBR):\n"\
"\n"\
"     \"preset=<кбита/с>\"  Този профил обикновено дава добро качество за\n"\
"                             зададения битрейт. В зависимост от указания\n"\
"                             битрейт, профилът ще определи оптималните за\n"\
"                             случая настройки.\n"\
"                             Въпреки че този метод върши работа, той не е\n"\
"                             толкова гъвкав, колкото VBR, и обикновено не\n"\
"                             достига качеството на VBR при високи битрейтове.\n"\
"\n"\
"Следните опции са достъпни за съответните профили:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Режим) - Не е нужно изрично да указвате ABR режима.\n"\
"                      За да го ползвате, просто укажете битрейт. Например:\n"\
"                      \"preset=185\" активира този\n"\
"                      профил и ползва средно 185 килобита в секунда.\n"\
"\n"\
"   \"fast\" - Разрешава новия, бърз VBR за определен профил. Недостаък на това\n"\
"            е, че за сметка на скоростта често полученият битрейт е по-висок,\n"\
"            а качеството дори по-ниско в сранение с нормалния режим на работа.\n"\
"   Внимание: С настоящата версия, полученият с бързия режим битрейт, може да се\n"\
"            окаже твърде висок, в сравнение с нормалните профили.\n"\
"\n"\
"   \"cbr\"  - Ако ползвате ABR режим (прочетете по-горе) със значителен\n"\
"            битрейт като 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            може да ползвате опцията \"cbr\" за да наложите кодиране в CBR\n"\
"            режим, вместо стандартния abr mode. ABR осигурява по-високо\n"\
"            качество, но CBR може да е по-подходящ в ситуации, като\n"\
"            предаването на mp3 през интернет поток.\n"\
"\n"\
"    Например:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" или \"-lameopts  cbr:preset=192       \"\n"\
" или \"-lameopts      preset=172       \"\n"\
" или \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Достъпни са някои псевдоними за ABR режим:\n"\
"phone => 16kbps/моно        phon+/lw/mw-eu/sw => 24kbps/моно\n"\
"mw-us => 40kbps/моно        voice => 56kbps/моно\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit "Не могат да се зададат LAME опциите, проверете битрейтовете/честотите на дискретите,"\
"някои много ниски битрейтове (<32) изискват ниски честоти на дискретите (напр. -srate 8000)."\
"Ако нищо друго не помага пробвайте някой preset."
#define MSGTR_ConfigFileError "грешка в конфигурационния файл"
#define MSGTR_ErrorParsingCommandLine "грешка при обработката на командния ред"
#define MSGTR_VideoStreamRequired "Задължително е да има видео поток!\n"
#define MSGTR_ForcingInputFPS "Входящите кадри в секунда ще се интерпретират като %5.3f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Изходния формат RAWVIDEO не поддържа аудио - звука се премахва\n"
#define MSGTR_DemuxerDoesntSupportNosound "Tози разпределител все още не поддържа -nosound .\n"
#define MSGTR_MemAllocFailed "не може да задели памет"
#define MSGTR_NoMatchingFilter "Не може да бъде намерен подходящ филтър/изходен аудио формат!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, може би заради C компилатора?\n"
#define MSGTR_NoLavcAudioCodecName "LAVC аудио, Липсва име на кодек!\n"
#define MSGTR_LavcAudioCodecNotFound "Aудио LAVC, не може да се намери енкодер за кодека %s\n"
#define MSGTR_CouldntAllocateLavcContext "Aудио LAVC, не може да задели контекст!\n"
#define MSGTR_CouldntOpenCodec "Не може да отвори кодек %s, br=%d\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     променлив битрейт метод\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(подразбира се)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           среден битрейт\n"\
"\n"\
" cbr           постоянен bitrate\n"\
"               Също така налага CBR кодиране за последователни ABR режими.\n"\
"\n"\
" br=<0-1024>   указва битрейта в КБита (само за CBR и ABR)\n"\
"\n"\
" q=<0-9>       качество (0-максимално, 9-минимално) (само за VBR)\n"\
"\n"\
" aq=<0-9>      качество на алгоритъма (0-най-добро/бавно, 9-най-лошо/бързо)\n"\
"\n"\
" ratio=<1-100> коефициент на компресия\n"\
"\n"\
" vol=<0-10>    усилване на входния звук\n"\
"\n"\
" mode=<0-3>    (по-подразбиране: автоматичен)\n"\
"                0: stereo\n"\
"                1: joint-стерео\n"\
"                2: двуканален\n"\
"                3: моно\n"\
"\n"\
" padding=<0-2>\n"\
"                0: без\n"\
"                1: всички\n"\
"                2: регулирано\n"\
"\n"\
" fast          По-бързо кодиране на последователни VBR режими,\n"\
"               малко по-ниско качество и по-високи битрейтове.\n"\
"\n"\
" preset=<value> Осигурява най-високото възможно качество при зададени настройки.\n"\
"                 medium: VBR  кодиране,  добро качество\n"\
"                 (150-180 КБита/с битрейт)\n"\
"                 standard:  VBR кодиране, високо качество\n"\
"                 (170-210 Кбита/с битрейт)\n"\
"                 extreme: VBR кодиране, много-високо качество\n"\
"                 (200-240 КБита/с битрейт)\n"\
"                 insane:  CBR  кодиране, най-високо качество\n"\
"                 (320 Кбита/с битрейт)\n"\
"                 <8-320>: ABR кодиране със зададен среден битрейт.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "дублиран FourCC код"
#define MSGTR_TooManyFourccs "твърде много FourCC кодoве/формати..."
#define MSGTR_ParseError "грешка при разчитане"
#define MSGTR_ParseErrorFIDNotNumber "грешка при разчитане (ID на формата не е число?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "грешка при разчитане (ID псевдонима на формата не е число?)"
#define MSGTR_DuplicateFID "дублирано ID на формата"
#define MSGTR_TooManyOut "твърде много изходни формати..."
#define MSGTR_InvalidCodecName "\nкодекът(%s) има невалидно име!\n"
#define MSGTR_CodecLacksFourcc "\nкодекът(%s) няма FourCC код/формат!\n"
#define MSGTR_CodecLacksDriver "\nкодекът(%s) няма драйвер!\n"
#define MSGTR_CodecNeedsDLL "\nкодекът(%s) се нуждае от 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nкодекът(%s) се нуждае от 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Не може да се задели памет за коментар. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Четене от %s: "
#define MSGTR_CantOpenFileError "'%s': %s не може да бъде отворен\n"
#define MSGTR_CantGetMemoryForLine "Няма достатъчно памет за 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Не може да презадели памет за '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Името на кодека '%s' не е уникално."
#define MSGTR_CantStrdupName "Не може да се изпълни strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Не може да се изпълни strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Не може да се изпълни strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Не може да се изпълни strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d аудио & %d видео кодека\n"
#define MSGTR_CodecDefinitionIncorrect "Кодекът не е дефиниран коректно."
#define MSGTR_OutdatedCodecsConf "Tози codecs.conf е твърде стар и несъвместим с тази версия на MPlayer!"

// fifo.c
#define MSGTR_CannotMakePipe "Не може да се създаде програмен канал (PIPE)!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Твърде стар save slot е открит в lvl %d: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Опцията %s не може да се използва в конфигурационен файл.\n"
#define MSGTR_InvalidCmdlineOption "Опцията %s не може да се ползва от командния ред.\n"
#define MSGTR_InvalidSuboption "Грешка: опцията '%s' няма подопция '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Грешка: подопцията '%s' на '%s' изисква параметър!\n"
#define MSGTR_MissingOptionParameter "Грешка: опцията '%s' изисква параметър!\n"
#define MSGTR_OptionListHeader "\n Име                 Вид            Мин        Mакс      Global  CL    Конф\n\n"
#define MSGTR_TotalOptions "\nОбщо: %d опции\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM устройство '%s' не е открито.\n"
#define MSGTR_ErrTrackSelect "Грешка при избор на VCD пътечка."
#define MSGTR_ReadSTDIN "Четене от стандартния вход (stdin)...\n"
#define MSGTR_UnableOpenURL "URL адреса не може да бъде отворен: %s\n"
#define MSGTR_ConnToServer "Установена е връзка със сървъра: %s\n"
#define MSGTR_FileNotFound "Файла не е намерен: '%s'\n"

#define MSGTR_SMBInitError "Библиотеката libsmbclient не може да бъде инициализирана: %d\n"
#define MSGTR_SMBFileNotFound "'%s' не може да бъде отворен през LAN\n"
#define MSGTR_SMBNotCompiled "MPlayer не е компилиран със поддръжка на четене от SMB.\n"

#define MSGTR_CantOpenDVD "Не може да бъде отворено DVD устройство: %s (%s)\n"
#define MSGTR_DVDnumTitles "Има %d заглавия на това DVD.\n"
#define MSGTR_DVDinvalidTitle "Невалиден номер на DVD заглавие: %d\n"
#define MSGTR_DVDnumChapters "Има %d раздела в това DVD заглавие.\n"
#define MSGTR_DVDinvalidChapter "Невалиден номер на DVD раздел: %d\n"
#define MSGTR_DVDnumAngles "Има %d гледни точки в това DVD заглавие..\n"
#define MSGTR_DVDinvalidAngle "Невалиден номер на гледна точка: %d\n"
#define MSGTR_DVDnoIFO "Не може да бъде отворен IFO файла на това DVD заглавие %d.\n"
#define MSGTR_DVDnoVOBs "Заглавието не може да бъде отворено (VTS_%02d_1.VOB).\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "ПРЕДУПРЕЖДЕНИЕ: Заглавната част на аудио потока %d е редефинирана.\n"
#define MSGTR_VideoStreamRedefined "ПРЕДУПРЕЖДЕНИЕ: Заглавната част на видео потока %d е редефинирана.\n"
#define MSGTR_TooManyAudioInBuffer "\nTвърде много аудио пакети в буфера: (%d в %d байта).\n"
#define MSGTR_TooManyVideoInBuffer "\nТвърде много видео пакети в буфера: (%d в %d байта).\n"
#define MSGTR_MaybeNI "Може би възпроизвеждате non-interleaved поток/файл или кодекът не се е справил?\n" \
		      "За AVI файлове, опитайте да наложите non-interleaved режим със опцията -ni.\n"
#define MSGTR_SwitchToNi "\nЗле структуриран AVI файл - превключване към -ni режим...\n"
#define MSGTR_Detected_XXX_FileFormat "%s формат.\n"
#define MSGTR_DetectedAudiofile "Аудио файл.\n"
#define MSGTR_NotSystemStream "Не е MPEG System Stream... (може би Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Невалиден MPEG-ES поток??? Свържете се с автора, може да е бъг :(\n"
#define MSGTR_FormatNotRecognized "============ За съжаление, този формат не се разпознава/поддържа =============\n"\
				  "=== Ако този файл е AVI, ASF или MPEG поток, моля уведомете автора! ===\n"
#define MSGTR_MissingVideoStream "Не е открит видео поток.\n"
#define MSGTR_MissingAudioStream "Не е открит аудио поток -> няма звук.\n"
#define MSGTR_MissingVideoStreamBug "Липсва видео поток!? Свържете се с автора, може да е бъг :(\n"

#define MSGTR_DoesntContainSelectedStream "разпределител: Файлът не съдържа избрания аудио или видео поток.\n"

#define MSGTR_NI_Forced "Наложен"
#define MSGTR_NI_Detected "Определен"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI файл.\n"

#define MSGTR_UsingNINI "Използва се NON-INTERLEAVED AVI формат.\n"
#define MSGTR_CouldntDetFNo "Не може да се определи броя на кадрите (за превъртане).\n"
#define MSGTR_CantSeekRawAVI "Не могат да се превъртат сурови AVI потоци. (Изисква се индекс, опитайте с -idx .)\n"
#define MSGTR_CantSeekFile "Този файл не може да се превърта.\n"

#define MSGTR_MOVcomprhdr "MOV: Поддръжката на компресирани хедъри изисква ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: ВНИМАНИЕ: Открит е променлив FOURCC код!?\n"
#define MSGTR_MOVtooManyTrk "MOV: ВНИМАНИЕ: твърде много пътечки"
#define MSGTR_FoundAudioStream "==> Открит е аудио поток: %d\n"
#define MSGTR_FoundVideoStream "==> Открит е видео поток: %d\n"
#define MSGTR_DetectedTV "Открита е телевизия! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Не може да бъде отворен ogg разпределител.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Търсене на звуков поток (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Не може да се отвори звуков поток: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Не могат да бъдат отворени субтитри: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Не може да бъде отворен аудио разпределител: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Не може да бъде отворен разпределител на субтитри: %s\n"
#define MSGTR_TVInputNotSeekable "Телевизията не може да се превърта! (Могат да се превключват евентуално каналите ;)\n"
#define MSGTR_ClipInfo "Информация за клипа:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30000/1001fps NTSC съдържание, превключване на кадровата честота.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24000/1001fps прогресивен NTSC, превключване на кадровата честота.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не може да бъде отворен кодек.\n"
#define MSGTR_CantCloseCodec "Не може да бъде затворен кодек.\n"

#define MSGTR_MissingDLLcodec "ГРЕШКА: необходимият DirectShow кодек %s не може да бъде отворен.\n"
#define MSGTR_ACMiniterror "Не може да се зареди/инициализира Win32/ACM АУДИО кодек (липсващ DLL файл?).\n"
#define MSGTR_MissingLAVCcodec "Не може да бъде открит кодек '%s' в libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: ФАТАЛНО: Достигнат е края на файла, по-време на търсене за sequence header.\n"
#define MSGTR_CannotReadMpegSequHdr "ФАТАЛНО: Не може да бъде прочетен sequence header.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не може да бъде прочетено разширението на sequence header.\n"
#define MSGTR_BadMpegSequHdr "MPEG: лош sequence header\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: лошо разширение на sequence header\n"

#define MSGTR_ShMemAllocFail "Не може да се задели споделена памет.\n"
#define MSGTR_CantAllocAudioBuf "Не може да се задели аудио буфер.\n"

#define MSGTR_UnknownAudio "Неизвестен/липсващ аудио формат -> няма звук\n"

#define MSGTR_UsingExternalPP "[PP] Използване на външен филтър за допълнителна обработка, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Използване на допълнителна обработка от страна на кодека, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Видео атрибут '%s' не се поддържа от vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Заявената фамилия видео кодеци [%s] (vfm=%s) не е достъпна.\nРазрешете я по време на компилация.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Заявената фамилия аудио кодеци [%s] (afm=%s) не е достъпна.\nРазрешете я по време на компилация.\n"
#define MSGTR_OpeningVideoDecoder "Отваряне на видео декодер: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Отваряне на аудио декодер: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "Инициализацията на VDecoder се провали :(\n"
#define MSGTR_ADecoderInitFailed "Инициализацията на ADecoder се провали :(\n"
#define MSGTR_ADecoderPreinitFailed "Предварителната инициализация на ADecoder се провали :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Заделяне на %d байта за входния буфер.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Заделяне на %d + %d = %d байта за изходния буфер.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Установяване на LIRC поддръжка...\n"
#define MSGTR_LIRCopenfailed "Няма да има LIRC поддръжка.\n"
#define MSGTR_LIRCcfgerr "Конфигурационният файл за LIRC %s не може да бъде прочетен.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Не може да бъде открит видео филтър '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Не може да бъде отворен видео филтър '%s'.\n"
#define MSGTR_OpeningVideoFilter "Отваряне на видео филтър: "
#define MSGTR_CannotFindColorspace "Не може да бъде открит съответстващ цветови формат, дори с вмъкване на 'scale':(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Кодекът не е указал sh->disp_w и sh->disp_h, опит за решение.\n"
#define MSGTR_VoConfigRequest "VDec: заявка на vo config - %d x %d (preferred csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Не е открит подходящ цветови формат - повторен опит с -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Пропорциите на филма са %.2f:1 - мащабиране до правилните пропорции .\n"
#define MSGTR_MovieAspectUndefined "Не са дефинирани пропорции - без предварително мащабиране.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Трябва да обновите/инсталирате пакета с двоичните кодеци.\nОтидете на http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Видеокодек Win32/DShow е инициализиран успешно.\n"
#define MSGTR_DMOInitOK "INFO: Видеокодек Win32/DMO е инициализиран успешно.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Не може да прати EWMH fullscreen Event!\n"

#define MSGTR_InsertingAfVolume "[Смесител] Няма хардуерно смесване, вмъкване на филтър за силата на звука.\n"
#define MSGTR_NoVolume "[Смесител] Не е достъпна настройка на звука.\n"

// ====================== GUI messages/buttons ========================

#ifdef CONFIG_GUI

// --- labels ---
#define MSGTR_About "Информация"
#define MSGTR_FileSelect "Избор на файл..."
#define MSGTR_SubtitleSelect "Избор на субтитри..."
#define MSGTR_OtherSelect "Избор..."
#define MSGTR_AudioFileSelect "Избор на външен аудио канал..."
#define MSGTR_FontSelect "Избор на шрифт..."
#define MSGTR_PlayList "Списък за възпроизвеждане"
#define MSGTR_Equalizer "Еквалайзер"
#define MSGTR_SkinBrowser "Избор на Skin"
#define MSGTR_Network "Поток от мрежата..."
#define MSGTR_Preferences "Предпочитания"
#define MSGTR_AudioPreferences "Конфигуриране на аудио драйвера"
#define MSGTR_NoMediaOpened "Няма отворени елементи."
#define MSGTR_VCDTrack "VCD писта %d"
#define MSGTR_NoChapter "Няма раздели"
#define MSGTR_Chapter "Раздел %d"
#define MSGTR_NoFileLoaded "Не е зареден файл."

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Отказ"
#define MSGTR_Add "Добавяне"
#define MSGTR_Remove "Премахване"
#define MSGTR_Clear "Изчистване"
#define MSGTR_Config "Конфигурация"
#define MSGTR_ConfigDriver "Конфигуриране на драйвера"
#define MSGTR_Browse "Избор"

// --- error messages ---
#define MSGTR_NEMDB "За съжаление, няма достатъчно памет за draw buffer."
#define MSGTR_NEMFMR "За съжаление, няма достатъчно памет за менюто."
#define MSGTR_IDFGCVD "За съжаление, няма съвместим с GUI видео драйвер."
#define MSGTR_NEEDLAVC "За съжаление, не можете да възпроизвеждате различни от MPEG\nфайлове с вашето DXR3/H+ устройство без прекодиране.\nМоля разрешете lavc в полето за конфигурация на DXR3/H+ ."
#define MSGTR_UNKNOWNWINDOWTYPE "Неизвестен тип на прозорец ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] грешка в конфигурационния файл на skin-а на ред %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] внимание в конфигурационния файл на ред %d:\nоткрит widget (%s) без \"section\" преди това"
#define MSGTR_SKIN_WARNING2 "[skin] внимание в конфигурациония файл на ред %d:\nоткрит widget (%s) без \"subsection\" преди това"
#define MSGTR_SKIN_WARNING3 "[skin] внимание в конфигурационния файл на ред %d:\nтази подсекция не се поддържа от widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] файлът ( %s ) не е намерен.\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmap с 16 и по-малко бита за цвят не се поддържа (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "файлът не е намерен (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "грешка при четене на BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "грешка при четене на TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "грешка при четене на PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "TGA с RLE компресия не се поддържа (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "неизвестен вид на файла (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "Грешка при преобразуване от 24 към 32 бита (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "неизвестно съобщение: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "недостатъчно памет\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Декларирани са твърде много шрифтове.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Файлът със шрифта не е намерен.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Файл с изображението на шрифта не е намерен.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "несъществуващ идентификатор на шрифт (%s)\n"
#define MSGTR_SKIN_UnknownParameter "неизвестен параметър (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Скинът не е намерен (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Избраният скин ( %s ) не е намерен, ще се ползва 'default'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Грешка в конфигурационен файл (%s)\n"
#define MSGTR_SKIN_LABEL "Скинове:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Относно MPlayer"
#define MSGTR_MENU_Open "Oтваряне..."
#define MSGTR_MENU_PlayFile "Пускане на файл..."
#define MSGTR_MENU_PlayVCD "Пускане на VCD..."
#define MSGTR_MENU_PlayDVD "Пускане на DVD..."
#define MSGTR_MENU_PlayURL "Пускане от URL..."
#define MSGTR_MENU_LoadSubtitle "Зареждане на субтитри..."
#define MSGTR_MENU_DropSubtitle "Премахване на субтитри..."
#define MSGTR_MENU_LoadExternAudioFile "Зареждане на външен звуков файл..."
#define MSGTR_MENU_Playing "Playing"
#define MSGTR_MENU_Play "Старт"
#define MSGTR_MENU_Pause "Пауза"
#define MSGTR_MENU_Stop "Стоп"
#define MSGTR_MENU_NextStream "Следващ"
#define MSGTR_MENU_PrevStream "Предишен"
#define MSGTR_MENU_Size "Размер"
#define MSGTR_MENU_HalfSize   "Половин размер"
#define MSGTR_MENU_NormalSize "Нормален размер"
#define MSGTR_MENU_DoubleSize "Двоен размер"
#define MSGTR_MENU_FullScreen "На цял екран"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Oтваряне на диск..."
#define MSGTR_MENU_ShowDVDMenu "Показване на DVD меню"
#define MSGTR_MENU_Titles "Заглавия"
#define MSGTR_MENU_Title "Заглавие %2d"
#define MSGTR_MENU_None "(няма)"
#define MSGTR_MENU_Chapters "Раздели"
#define MSGTR_MENU_Chapter "Раздел %2d"
#define MSGTR_MENU_AudioLanguages "Език за аудио"
#define MSGTR_MENU_SubtitleLanguages "Език на субтитрите"
// TODO: Why is this different from MSGTR_PlayList?
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Избор на Skin"
// TODO: Why is this different from MSGTR_Preferences?
#define MSGTR_MENU_Preferences "Настройки"
#define MSGTR_MENU_Exit "Изход..."
#define MSGTR_MENU_Mute "Без звук"
#define MSGTR_MENU_Original "Без промяна"
#define MSGTR_MENU_AspectRatio "Съотношение"
#define MSGTR_MENU_AudioTrack "Аудио писта"
#define MSGTR_MENU_Track "Писта %d"
#define MSGTR_MENU_VideoTrack "видео писта"

// --- equalizer
#define MSGTR_EQU_Audio "Аудио"
#define MSGTR_EQU_Video "Видео"
#define MSGTR_EQU_Contrast "Контраст: "
#define MSGTR_EQU_Brightness "Светлост: "
#define MSGTR_EQU_Hue "Тон: "
#define MSGTR_EQU_Saturation "Наситеност: "
#define MSGTR_EQU_Front_Left "Преден Ляв"
#define MSGTR_EQU_Front_Right "Преден Десен"
#define MSGTR_EQU_Back_Left "Заден Ляв"
#define MSGTR_EQU_Back_Right "Заден Десен"
#define MSGTR_EQU_Center "Централен"
#define MSGTR_EQU_Bass "Бас"
#define MSGTR_EQU_All "Всички"
#define MSGTR_EQU_Channel1 "Канал 1:"
#define MSGTR_EQU_Channel2 "Канал 2:"
#define MSGTR_EQU_Channel3 "Канал 3:"
#define MSGTR_EQU_Channel4 "Канал 4:"
#define MSGTR_EQU_Channel5 "Канал 5:"
#define MSGTR_EQU_Channel6 "Канал 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Път"
#define MSGTR_PLAYLIST_Selected "Избрани файлове"
#define MSGTR_PLAYLIST_Files "Файлове"
#define MSGTR_PLAYLIST_DirectoryTree "Директории"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Субтитри и OSD"
#define MSGTR_PREFERENCES_Codecs "Кодеци & demuxer"
#define MSGTR_PREFERENCES_Misc "Разни"

#define MSGTR_PREFERENCES_None "Без"
#define MSGTR_PREFERENCES_DriverDefault "Подразбиращи се за драйвера"
#define MSGTR_PREFERENCES_AvailableDrivers "Достъпни драйвери:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Без звук"
#define MSGTR_PREFERENCES_NormalizeSound "Изравняване на звука"
#define MSGTR_PREFERENCES_EnableEqualizer "Включване на еквалайзера"
#define MSGTR_PREFERENCES_SoftwareMixer "Включва Софтуерен Смесител"
#define MSGTR_PREFERENCES_ExtraStereo "Включване на допълнително стерео"
#define MSGTR_PREFERENCES_Coefficient "Коефициент:"
#define MSGTR_PREFERENCES_AudioDelay "Закъснение на звука"
#define MSGTR_PREFERENCES_DoubleBuffer "Двойно буфериране"
#define MSGTR_PREFERENCES_DirectRender "Включване на direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Разрешаване на прескачането на кадри"
#define MSGTR_PREFERENCES_HFrameDrop "Разрешаване на ИНТЕНЗИВНО прескачане на кадри (опасно)"
#define MSGTR_PREFERENCES_Flip "Преобръщане на образа"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Часовник и индикатори"
#define MSGTR_PREFERENCES_OSDProgress "Само индикатори за напредване"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Часовник, проценти и общо време"
#define MSGTR_PREFERENCES_Subtitle "Субтитри:"
#define MSGTR_PREFERENCES_SUB_Delay "Закъснение: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Местоположение: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Изключване на автоматичното зареждане на субтитри"
#define MSGTR_PREFERENCES_SUB_Unicode "Субтитри с Unicode кодиране"
#define MSGTR_PREFERENCES_SUB_MPSUB "Преобразуване на субтитрите в формата на MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Преобразуване на субтитрите в SubViewer (SRT) формат"
#define MSGTR_PREFERENCES_SUB_Overlap "Препокриване на субтитрите"
#define MSGTR_PREFERENCES_Font "Шрифт:"
#define MSGTR_PREFERENCES_FontFactor "Дебелина на сянката на шрифта:"
#define MSGTR_PREFERENCES_PostProcess "Разрешаване на допълнителна обработка"
#define MSGTR_PREFERENCES_AutoQuality "Автоматичен контрол на качеството: "
#define MSGTR_PREFERENCES_NI "Разчитане на non-interleaved AVI формат"
#define MSGTR_PREFERENCES_IDX "Построяване на индексната таблица наново, при необходимост"
#define MSGTR_PREFERENCES_VideoCodecFamily "Фамилия видео кодеци:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Фамилия аудио кодеци:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD степен"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Субтитри"
#define MSGTR_PREFERENCES_FRAME_Font "Шрифт"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Допълнителна обработка"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Кодек & разпределител"
#define MSGTR_PREFERENCES_FRAME_Cache "Кеширане"
#define MSGTR_PREFERENCES_Audio_Device "Устройство:"
#define MSGTR_PREFERENCES_Audio_Mixer "Смесител:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Канал на смесителя:"
#define MSGTR_PREFERENCES_Message "Не забравяйте, да рестартирате възпроизвеждането за да влязат в сила някои опции!"
#define MSGTR_PREFERENCES_DXR3_VENC "Видео енкодер:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Използване на LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Западноевропейски Езици (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Западноевропейски Езици със Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Славянски/Централноевропейски Езици (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Есперанто, Галски, Малтийски, Турски (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Стар Балтийски (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Кирилица (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Арабски (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Съвременен Гръцки (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Турски (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Балтийски (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Келтски (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebrew charsets (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Руски (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Украински, Беларуски (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Опростен Китайски (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Традиционен Китайски (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Японски (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Kорейски (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai charset (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Кирилица Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Славянски/Централноевропейски Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Без автоматично мащабиране"
#define MSGTR_PREFERENCES_FontPropWidth "Пропорционално на широчината на филма"
#define MSGTR_PREFERENCES_FontPropHeight "Пропорционално на височината на филма"
#define MSGTR_PREFERENCES_FontPropDiagonal "Пропорционално на дължината на диагонала"
#define MSGTR_PREFERENCES_FontEncoding "Кодировка:"
#define MSGTR_PREFERENCES_FontBlur "Размазване:"
#define MSGTR_PREFERENCES_FontOutLine "Удебеляване:"
#define MSGTR_PREFERENCES_FontTextScale "Мащаб на текста:"
#define MSGTR_PREFERENCES_FontOSDScale "Мащаб на OSD:"
#define MSGTR_PREFERENCES_Cache "Кеширане"
#define MSGTR_PREFERENCES_CacheSize "Размер на кеша: "
#define MSGTR_PREFERENCES_LoadFullscreen "Стартиране на цял екран"
#define MSGTR_PREFERENCES_SaveWinPos "Запаметяване на местоположението на прозореца"
#define MSGTR_PREFERENCES_XSCREENSAVER "Изключване на XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Лента за превъртане"
#define MSGTR_PREFERENCES_AutoSync "Автоматична синхронизация"
#define MSGTR_PREFERENCES_AutoSyncValue "Степен на синхронизацията: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM устройство:"
#define MSGTR_PREFERENCES_DVDDevice "DVD устройство:"
#define MSGTR_PREFERENCES_FPS "Кадри в секунда:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Показване на видео прозореца при неактивност"

#define MSGTR_ABOUT_UHU "Разработката на графичния интерфейс се спонсорира от UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Фатална грешка!"
#define MSGTR_MSGBOX_LABEL_Error "Грешка!"
#define MSGTR_MSGBOX_LABEL_Warning "Внимание!"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] недостатъчно памет за изображението\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] недостатъчно памет за изображението\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] грешка при четене на конфигурационния файл ...\n"
#define MSGTR_UnableToSaveOption "Не може да се запамети опцията '%s'.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Изтриване на субтитрите.\n"
#define MSGTR_LoadingSubtitles "[GUI] Зареждане на субтитрите: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Добавяне на видео филтър: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Премахване на видео филтър: %s\n"

// mw.c

#define MSGTR_NotAFile "Това не прилича на файл: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] Не може да бъде отворен DISPLAY.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Отдалечен дисплей, изключване на  XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] За съжаление вашата система не поддържа разширението на X за споделена памет.\n"
#define MSGTR_WS_NoXshape "[ws] За съжаление вашата система не поддържа разширението XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Твърде ниска дълбочина на цветовете.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Твърде много отворени прозорци.\n"
#define MSGTR_WS_ShmError "[ws] грешка в разширението за споделена памет\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Няма достатъчно памет за draw buffer.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS не е достъпен?\n"
#define MSGTR_WS_DpmsNotEnabled "DPMS не може да бъде включен.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Това не прилича на файл...\n"
#define MSGTR_WS_DDNothing "D&D: Не е върнат резултат!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Избраното изходно видео устройство е несъвместимо с този кодек.\n"
#define MSGTR_VO_GenericError "Tази грешка е възникнала"
#define MSGTR_VO_UnableToAccess "Достъпът е невъзможен"
#define MSGTR_VO_ExistsButNoDirectory "вече съществува, но не е директория."
#define MSGTR_VO_DirExistsButNotWritable "Директорията съществува, но не е разрешен запис."
#define MSGTR_VO_DirExistsAndIsWritable "Директорията съществува и е разрешена за запис."
#define MSGTR_VO_CantCreateDirectory "Директорията не може да бъде създадена."
#define MSGTR_VO_CantCreateFile "Файлът не може да бъде създаден."
#define MSGTR_VO_DirectoryCreateSuccess "Директорията е успешно създадена."
#define MSGTR_VO_ParsingSuboptions "Обработка на подопциите."
#define MSGTR_VO_SuboptionsParsedOK "Завърши обработката на подопциите."
#define MSGTR_VO_ValueOutOfRange "Стойността е извън допустимите граници"
#define MSGTR_VO_NoValueSpecified "Не е указана стойност."
#define MSGTR_VO_UnknownSuboptions "Неизвестна подопция(и)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nТова са подопциите на aalib vo_aa:\n"
#define MSGTR_VO_AA_AdditionalOptions "Допълнителни опции предвидени от vo_aa:\n" \
"  help        показва това съобщение\n" \
"  osdcolor    задава цвят за osd\n  subcolor    задава цвета на субтитрите\n" \
"        параметрите за цвят са:\n           0 : нормален\n" \
"           1 : dim\n           2 : удебелен\n           3 : удебелен шрифт\n" \
"           4 : обърнат\n           5 : специален\n\n\n" 


// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Включен е progressive JPEG формат."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Progressive JPEG форматът е изключен."
#define MSGTR_VO_JPEG_BaselineJPEG "Включен е baseline JPEG формат."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline JPEG форматът е изключен."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Включен е ASCII режим."
#define MSGTR_VO_PNM_RawMode "Включен е \"суров\" режим."
#define MSGTR_VO_PNM_PPMType "Ще записва в PPM файлове."
#define MSGTR_VO_PNM_PGMType "Ще записва в PGM файлове."
#define MSGTR_VO_PNM_PGMYUVType "Ще записва в PGMYUV файлове."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Режимът interlaced изисква височината на образа да е кратна на  4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Не може да се задели буфер за редовете за interlaced режим."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Входния формат не е RGB, не могат да се отделят цветовите полета!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Широчината на образа трябва да е кратна на 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Няма достатъчно памет за RGB кадров буфер."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Не е получена памет или файлов манипулатор за запис \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Грешка при извеждане на изображението!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Неизвестно подустройство: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Използване на interlaced изходен режим, от горе на долу."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Използване на interlaced изходен режим, от долу на горе."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Използва се (подразбиращ се) прогресивен режим"

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "pgm видео драйвера е заменен от -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "md5 видео драйвера е заменен от -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: модулите alsa9 и alsa1x са отстранени, използвайте -ao alsa .\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Не може да отвори устройство смесител %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup:\nСмесителят на звуковата карта няма канал '%s', използва се подразбиращ се.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Аудио устройство %s не може да бъде отворено: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Не може да бъде създаден файлов дескриптор: %s\n"
//#define MSGTR_AO_OSS_CantSetAC3 "[AO OSS] Не може да се зададе за устройство %s формат AC3, опит с S16...\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Аудио устройство %s не може да бъде настроено за %s извеждане, проба с %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Не може да настрои звуковата карта за %d канала.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: драйверът не поддържа SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Вашият аудио драйвер НЕ поддържа функцията select()  ***\n Рекомпилирайте MPlayer с #undef HAVE_AUDIO_SELECT в config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS] Фатална грешка:\n *** НЕ МОЖЕ ДА ПРЕ-ОТВОРИ/РЕСТАРТИРА АУДИО УСТРОЙСТВОТО *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Установена е връзка със аудио сървъра.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Потокът не може да бъде отворен.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Потокът е отворен.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] размер на буфера: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Силата на звука не може да бъде сменена на %d.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz не се поддържат, опитайте \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound се провали: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] закъснение: [сървър: %0.2fс, мрежа: %0.2fс] (настройка %0.2fс)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Не може да бъде отворен esd поток за възпроизвеждане: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB audio set mixer се провали: %s\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz не се поддържат, опитайте с resample...\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] File: %s (%s)\nPCM: Честота: %iHz Канали: %s Формат %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: най-бързо извличане се постига с -vc null -vo null\nPCM: Info: за да запишете WAVE файлове ползвайте -ao pcm:waveheader (подразбира се).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] %s не може да се отвори за запис!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Честота: %iHz Канали: %s Формат %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] използва се %s аудио драйвер.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Неподдържан аудио формат: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Инициализацията на SDL Аудио се провали: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Аудиото не може да се отвори: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] контрол.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Честота: %iHz Канали: %s Формат %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: невалидно устройство.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams се провали: %s\nНе може да се зададе разчитаната честота.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE не се възприема от посоченото устройство.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams се провали: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: честотата на дискретизация е %lf (разчитаната честота е %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Не може да бъде отворен аудио канал: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO се провали.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: провал на записа."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Не може да бъде отворено устройство %s, %s  -> без звук.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: вашата звукова карта не поддържа %d канал, %s, %d Hz честота.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Вашият аудио драйвер НЕ поддържа функцията select()  ***\nРекомпилирайте MPlayer с #undef HAVE_AUDIO_SELECT в config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]Фатална грешка:\n *** АУДИО УСТРОЙСТВОТО (%s) НЕ МОЖЕ ДА БЪДЕ ПРЕ-ОТВОРЕНО/РЕСТАРТИРАНО ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: заявен формат: %d Hz, %d канала, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: не са открити звукови карти.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: заявен е невалиден формат (%s) - отхвърлен.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: грешка при отваряне за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: pcm info грешка: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d звукови карти са открити, ползва се: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: pcm channel info грешка: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: грешка при настройване на параметрите: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: грешка при настройка на канал: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: грешка при подготовка на канал: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: грешка при изчистване потока за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: грешка при възстановяване на буферите за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: грешка при затваряне на pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: грешка при изчистване на потока за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: грешка при възстановяване на буферите за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: грешка при подготовка на канал: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: грешка при изчистване на потока за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: грешка при възстановяване на буферите за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: грешка при подготовка на канал: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: претоварване на alsa, рестартиране на потока.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: грешка при подготовка за възпроизвеждане: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: грешка при запис след рестартиране: %s - отказ от операцията.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: грешка на изхода: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] невалиден плъгин: %s\n"

// ======================= AF Audio Filters ================================

// libaf

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "достъпни етикети в"
#define MSGTR_AF_LADSPA_WarnNoInputs "ВНИМАНИЕ! Този LADSPA плъгин не приема аудио.\n  Пристигащият аудио сигнал ще бъде загубен."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Многоканални (>2) плъгини не се поддържат (все още).\n  Използвайте само моно и стерео плъгини."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Този LADSPA плъгин не извежда звук."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Броя на аудио входовете на този LADSPA плъгин се различава от броя на аудио изходите."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "не може да се зареди"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Функцията ladspa_descriptor() не може да бъде открита в указания библиотечен файл."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Етикета не може да бъде намерен в библиотеката."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Не са указани подопции"
#define MSGTR_AF_LADSPA_ErrNoLibFile "Не е указан файл с библиотека"
#define MSGTR_AF_LADSPA_ErrNoLabel "Не е указан етикет на филтър"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Не са указани достатъчно контроли от командния ред"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Input control #%d е под долната граница от %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Input control #%d е над горната граница от %0.4f.\n"
