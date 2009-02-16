/*  Translated by:  Volodymyr M. Lisivka <lvm@mystery.lviv.net>,
	  Andriy Gritsenko <andrej@lucky.net>
    sevenfourk <sevenfourk@gmail.com>
   Was synced with help_mp-en.h: rev 1.105

========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Запуск:   mplayer [опції] [path/]filename\n"
"\n"
"Опції:\n"
" -vo <drv[:dev]> вибір драйвера і пристрою відео виводу (список див. з '-vo help')\n"
" -ao <drv[:dev]> вибір драйвера і пристрою аудіо виводу (список див. з '-ao help')\n"
#ifdef CONFIG_VCD
" vcd://<номер треку> грати VCD (video cd) трек з пристрою замість файлу\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<номер титрів> грати DVD титри/трек з пристрою замість файлу\n"
#endif
" -alang/-slang   вибрати мову DVD аудіо/субтитрів (двосимвольний код країни)\n"
" -ss <час>       переміститися на задану (секунди або ГГ:ХХ:СС) позицію\n"
" -nosound        без звуку\n"
" -fs -vm -zoom   повноекранне програвання (повноекр.,зміна відео,масштабування\n"
" -x <x> -y <y>   маштабувати картинку до <x> * <y> [якщо -vo драйвер підтримує!]\n"
" -sub <file>     вказати файл субтитрів (див. також -subfps, -subdelay)\n"
" -playlist <file> вказати playlist\n"
" -vid x -aid y   опції для вибору відео (x) і аудіо (y) потоку для програвання\n"
" -fps x -srate y опції для зміни відео (x кадр/сек) і аудіо (y Hz) швидкості\n"
" -pp <quality>   дозволити фільтр (0-4 для DivX, 0-63 для mpegs)\n"
" -framedrop      дозволити втрату кадрів (для повільних машин)\n"
"\n"
"Клавіші:\n"
" <-  або ->      перемотування вперед/назад на 10 секунд\n"
" вверх або вниз  перемотування вперед/назад на  1 хвилину\n"
" pgup або pgdown перемотування вперед/назад на 10 хвилин\n"
" < або >         перемотування вперед/назад у списку програвання\n"
" p або ПРОБІЛ    зупинити фільм (будь-яка клавіша - продовжити)\n"
" q або ESC       зупинити відтворення і вихід\n"
" + або -         регулювати затримку звуку по +/- 0.1 секунді\n"
" o               циклічний перебір OSD режимів:  нема / навігація / навігація+таймер\n"
" * або /         додати або зменшити гучність (натискання 'm' вибирає master/pcm)\n"
" z або x         регулювати затримку субтитрів по +/- 0.1 секунді\n"
" r or t          змінити положення субтитрів вгору/вниз, також див. -vf expand\n"
"\n"
" * * * ДЕТАЛЬНІШЕ ДИВ. ДОКУМЕНТАЦІЮ, ПРО ДОДАТКОВІ ОПЦІЇ І КЛЮЧІ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 
#define MSGTR_Exiting "\nВиходимо...\n"
#define MSGTR_ExitingHow "\nВиходимо... (%s)\n"
#define MSGTR_Exit_quit "Вихід"
#define MSGTR_Exit_eof "Кінець файлу"
#define MSGTR_Exit_error "Фатальна помилка"
#define MSGTR_IntBySignal "\nMPlayer перерваний сигналом %d у модулі: %s \n"
#define MSGTR_NoHomeDir "Не можу знайти домашній каталог\n"
#define MSGTR_GetpathProblem "проблеми у get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Створення файлу конфігурації: %s\n"
#define MSGTR_BuiltinCodecsConf "Використовую вбудований codecs.conf\n"
#define MSGTR_CantLoadFont "Не можу завантажити шрифт: %s\n"
#define MSGTR_CantLoadSub "Не можу завантажити субтитри: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: обраний потік загублений!\n"
#define MSGTR_CantOpenDumpfile "Не можу відкрити файл дампу!!!\n"
#define MSGTR_CoreDumped "Створено дамп ядра :)\n"
#define MSGTR_FPSnotspecified "Не вказано чи невірна кількість кадрів, застосуйте опцію -fps.\n"
#define MSGTR_TryForceAudioFmtStr "Намагаюсь форсувати групу аудіо кодеків %s...\n"
#define MSGTR_CantFindVideoCodec "Не можу знайти кодек для відео формату 0x%X!\n"
#define MSGTR_RTFMCodecs "Прочитайте DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Намагаюсь форсувати групу відео кодеків %s...\n"
#define MSGTR_CannotInitVO "ФАТАЛЬНО: Не можу ініціалізувати відео драйвер!\n"
#define MSGTR_CannotInitAO "не можу відкрити/ініціалізувати аудіо пристрій -> ГРАЮ БЕЗ ЗВУКУ\n"
#define MSGTR_StartPlaying "Початок програвання...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ********************************************************\n"\
"         **** Ваша система надто ПОВІЛЬНА щоб відтворити це! ****\n"\
"         ********************************************************\n"\
"!!! Можливі причини, проблеми, обхідні шляхи: \n"\
"- Найбільш загальні: поганий/сирий _аудіо_ драйвер :\n"\
"  - спробуйте -ao sdl або використовуйте ALSA 0.5 або емуляцію oss на ALSA 0.9.\n"\
"  - Експеримент з різними значеннями для -autosync, спробуйте 30 .\n"\
"- Повільний відео вивід.\n"\
"  - спробуйте інший -vo драйвер (список: -vo help) або спробуйте з -framedrop!\n"\
"- Повільний ЦП. Не намагайтеся відтворювати великі dvd/divx на повільних\n"\
"  процесорах! спробуйте -hardframedrop\n"\
"- Битий файл. Спробуйте різні комбінації: -nobps  -ni  -mc 0  -forceidx\n"\
"- Повільний носій (диски NFS/SMB, DVD, VCD та ін.). Спробуйте -cache 8192.\n"\
"- Ви використовуєте -cache для програвання неперемеженого AVI файлу?\n"\
"  - спробуйте -nocache.\n"\
"Читайте поради в файлах DOCS/HTML/en/video.html .\n"\
"Якщо нічого не допомогло, тоді читайте DOCS/HTML/en/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer був скомпільований БЕЗ підтримки GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI вимагає X11!\n"
#define MSGTR_Playing "Програвання %s\n"
#define MSGTR_NoSound "Аудіо: без звуку!!!\n"
#define MSGTR_FPSforced "Примусово змінена кількість кадрів на секунду на %5.3f (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Скомпільвано з автовизначенням CPU - УВАГА - це не оптимально!\nДля отримання кращих результатів перекомпілюйте MPlayer з --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Скомпільовано для x86 CPU з розширеннями:"
#define MSGTR_AvailableVideoOutputDrivers "Доступні модулі відео виводу:\n"
#define MSGTR_AvailableAudioOutputDrivers "Доступні модулі аудіо виводу:\n"
#define MSGTR_AvailableAudioCodecs "Доступні аудіо кодеки:\n"
#define MSGTR_AvailableVideoCodecs "Доступні відео кодеки:\n"
#define MSGTR_AvailableAudioFm "Доступні (вбудовані) групи/драйвера аудіо кодеків:\n"
#define MSGTR_AvailableVideoFm "Доступні (вбудовані) групи/драйвера відео кодеків:\n"
#define MSGTR_AvailableFsType "Доступні варіанти повноекранного відеорежиму:\n"
#define MSGTR_UsingRTCTiming "Використовую апаратний таймер RTC (%ldГц).\n"
#define MSGTR_CannotReadVideoProperties "Відео: Неможливо отримати властивості.\n"
#define MSGTR_NoStreamFound "Потік не знайдено.\n"
#define MSGTR_ErrorInitializingVODevice "Помилка відкриття/ініціалізації вибраного video_out (-vo) пристрою.\n"
#define MSGTR_ForcedVideoCodec "Примусовий відео кодек: %s\n"
#define MSGTR_ForcedAudioCodec "Примусовий аудіо кодек: %s\n"
#define MSGTR_Video_NoVideo "Відео: без відео\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Неможливо ініціалізувати відео фільтри (-vf) або відео вивід (-vo).\n"
#define MSGTR_Paused "\n  =====  ПАУЗА  =====\r"
#define MSGTR_PlaylistLoadUnable "\nНеможливо завантажити playlist %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer зламався через 'Невірні інструкції'.\n"\
"  Може бути помилка у вашому новому коду визначення типу CPU...\n"\
"  Будь-ласка перегляньте DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer зламався через 'Невірні інструкції'.\n"\
"  Іноді таке трапляється під час запуску програвача на CPU що відрізняється від того, на якому він\n"\
"  був зібраний/оптимізований.\n  Перевірте!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer зламався через невірне використання CPU/FPU/RAM.\n"\
"  Зберіть знову MPlayer з --enable-debug а зробіть 'gdb' backtrace та\n"\
"  дизасемблювання. Для довідок, перегляньте DOCS/HTML/en/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer зламався. Цього не повинно було трапитися.\n"\
"  Може бути помилка у коді MPlayer _або_ ваших драйверах _або_ через\n"\
"  версію gcc. Якщо важаєте що, це помилка MPlayer, будь-ласка читайте\n"\
"  DOCS/HTML/en/bugreports.html та слідкуєте інструкціям. Ми можемо\n"\
"  допомогти лише у разі забезпечення інформація коли доповідаєте про помилку.\n"
#define MSGTR_LoadingConfig "Завантаження конфігурації '%s'\n"
#define MSGTR_LoadingProtocolProfile "Завантаження профілю для протоколу '%s'\n"
#define MSGTR_LoadingExtensionProfile "Завантаження профілю для розширення '%s'\n"
#define MSGTR_AddedSubtitleFile "СУБТИТРИ: Додано файл субтитрів (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "СУБТИТРИ: Видалено файл субтитрів (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Помилка при відкритті файлу [%s] для запису!\n"
#define MSGTR_CommandLine "Командний рядок:"
#define MSGTR_RTCDeviceNotOpenable "Не можу відкрити %s: %s (користувач повинен мати права читання для файлу.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Помилка ініцілізації Linux RTC у ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Спробуйте додати \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" до скриптів запуску системи.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Помилка ініціалізації Linux RTC у ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Використовую %s синхронізацію.\n"
#define MSGTR_NoIdleAndGui "Опція -idle не викориcтовується в GMPlayer.\n"
#define MSGTR_MenuInitialized "Меню ініціалізовано: %s\n"
#define MSGTR_MenuInitFailed "Ініціалізація меню невдале.\n"
#define MSGTR_Getch2InitializedTwice "ПОПЕРЕДЖЕННЯ: getch2_init визвано двічі!\n"
#define MSGTR_DumpstreamFdUnavailable "Не можу створити дамп цього потоку - не має доступного дексриптору.\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Не можу відкрити відео фільтр libmenu з цим кореневим меню %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Помилка у ланці pre-init аудіо фільтру!\n"
#define MSGTR_LinuxRTCReadError "Помилка читання Linux RTC: %s\n"
#define MSGTR_SoftsleepUnderflow "Попередження! Недупустиме низьке значення затримки програми!\n"
#define MSGTR_DvdnavNullEvent "Подія DVDNAV NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "Подія DVDNAV: Подія виділення зламана\n"
#define MSGTR_DvdnavEvent "Подія DVDNAV: %s\n"
#define MSGTR_DvdnavHighlightHide "Подія DVDNAV: Виділення сховано\n"
#define MSGTR_DvdnavStillFrame "######################################## Подія DVDNAV: Стоп-кадр: %d сек\n"
#define MSGTR_DvdnavNavStop "Подія DVDNAV: Зупинка Nav\n"
#define MSGTR_DvdnavNavNOP "Подія DVDNAV: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "Подія DVDNAV: Зміна SPU потоку Nav: фізично: %d/%d/%d логічно: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "Подія DVDNAV: Зміна SPU потоку Nav: фізично: %d логічно: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "Подія DVDNAV: Зміна Аудіо потоку Nav: фізично: %d логічно: %d\n"
#define MSGTR_DvdnavNavVTSChange "Подія DVDNAV: Зміна Nav VTS\n"
#define MSGTR_DvdnavNavCellChange "Подія DVDNAV: Зміна Nav Cell\n"
#define MSGTR_DvdnavNavSpuClutChange "Подія DVDNAV: Зміна Nav SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "Подія DVDNAV: Nav Seek зроблено\n"
#define MSGTR_MenuCall "Виклик меню\n"

#define MSGTR_EdlOutOfMem "Не можу виділити достатньо пам'яті для збереження даних EDL.\n"
#define MSGTR_EdlRecordsNo "Читання %d EDL дій.\n"
#define MSGTR_EdlQueueEmpty "Немає дій EDL які треба виконати.\n"
#define MSGTR_EdlCantOpenForWrite "Не може відкрити EDL файл [%s] для запису.\n"
#define MSGTR_EdlCantOpenForRead "Не може відкрити EDL файл [%s] для читання.\n"
#define MSGTR_EdlNOsh_video "Не можу використати EDL без відео, вимикаю.\n"
#define MSGTR_EdlNOValidLine "Невірний рядок EDL: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Погано відформатований EDL рядок [%d], пропускаю.\n"
#define MSGTR_EdlBadLineOverlap "Остання зупинка була [%f]; наступний старт [%f].\n"\
"Записи повинні бути у хронологічному порядку, не можу перекрити. Пропускаю.\n"
#define MSGTR_EdlBadLineBadStop "Час зупинки повинен бути після часу старту.\n"
#define MSGTR_EdloutBadStop "Ігнорування EDL відмінено, останній start > stop\n"
#define MSGTR_EdloutStartSkip "Старт EDL пропуску, натисніть 'i' знов, щоб завершити блок.\n"
#define MSGTR_EdloutEndSkip "Кінець EDL пропуску, рядок записано.\n"
#define MSGTR_MPEndposNoSizeBased "Опція -endpos у MPlayer ще не підтримує одиниці ромзіру.\n"

// mplayer.c OSD
#define MSGTR_OSDenabled "увімкнено"
#define MSGTR_OSDdisabled "вимкнено"
#define MSGTR_OSDAudio "Аудіо: %s"
#define MSGTR_OSDVideo "Відео: %s"
#define MSGTR_OSDChannel "Канал: %s"
#define MSGTR_OSDSubDelay "Затримка субтитрыв: %d мс"
#define MSGTR_OSDSpeed "Швидкість: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"
#define MSGTR_OSDChapter "Розділ: (%d) %s"
#define MSGTR_OSDAngle "Кут: %d/%d"

// property values
#define MSGTR_EnabledEdl "увімкнено (EDL)"
#define MSGTR_Disabled "вимкнено"
#define MSGTR_HardFrameDrop "інтенсивний"
#define MSGTR_Unknown "невідомий"
#define MSGTR_Bottom "низ"
#define MSGTR_Center "центр"
#define MSGTR_Top "верх"
#define MSGTR_SubSourceFile "файл"
#define MSGTR_SubSourceVobsub "vobsub"
#define MSGTR_SubSourceDemux "вкладено"

// OSD bar names
#define MSGTR_Volume "Гучність"
#define MSGTR_Panscan "Зріз сторін"
#define MSGTR_Gamma "Гамма"
#define MSGTR_Brightness "Яскравість"
#define MSGTR_Contrast "Контраст"
#define MSGTR_Saturation "Насиченність"
#define MSGTR_Hue "Колір"
#define MSGTR_Balance "Баланс"

// property state
#define MSGTR_LoopStatus "Повтор: %s"
#define MSGTR_MuteStatus "Вимкнути звук: %s"
#define MSGTR_AVDelayStatus "A-V затримка: %s"
#define MSGTR_OnTopStatus "Звурху інших: %s"
#define MSGTR_RootwinStatus "Вікно-root: %s"
#define MSGTR_BorderStatus "Рамка: %s"
#define MSGTR_FramedroppingStatus "Пропуск кадрів: %s"
#define MSGTR_VSyncStatus "Вертикальна синхронізація: %s"
#define MSGTR_SubSelectStatus "Субтитри: %s"
#define MSGTR_SubSourceStatus "Субтитри з: %s"
#define MSGTR_SubPosStatus "Позиція субтитрів: %s/100"
#define MSGTR_SubAlignStatus "Вирівнювання субтитрів: %s"
#define MSGTR_SubDelayStatus "Затримка субтитрів: %s"
#define MSGTR_SubScale "Масштаб субтитрів: %s"
#define MSGTR_SubVisibleStatus "Субтитри: %s"
#define MSGTR_SubForcedOnlyStatus "Форсувати тільки субтитри: %s"

// mencoder.c:
#define MSGTR_UsingPass3ControlFile "Використовую pass3 файл: %s\n"
#define MSGTR_MissingFilename "\nНевизначений файл.\n\n"
#define MSGTR_CannotOpenFile_Device "Неможливо відкрити файл/пристрій.\n"
#define MSGTR_CannotOpenDemuxer "Неможливо відкрити demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nНе вибраний аудіо кодек (-oac). Виберіть або використовуйте -nosound. Спробуйте -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\nНе вибраний відео кодек (-ovc). Виберіть, спробуйте -ovc help!\n"
#define MSGTR_CannotOpenOutputFile "Неможливо створити файл '%s'.\n"
#define MSGTR_EncoderOpenFailed "Неможливо відкрити кодек.\n"
#define MSGTR_MencoderWrongFormatAVI "\nПОПЕРЕДЖЕННЯ: ФОРМАТ ФАЙЛУ НА ВИХОДІ _AVI_. Погляньте -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nПОПЕРЕДЖЕННЯ: ФОРМАТ ФАЙЛУ НА ВИХОДІ _MPEG_. Погляньте -of help.\n"
#define MSGTR_MissingOutputFilename "Не вказано файлу на виході, будь-ласка подивіться опцію -o."
#define MSGTR_ForcingOutputFourcc "Встановлюю вихідний fourcc в %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "Форсую таг аудіо фармату на виході до 0x%x.\n"
#define MSGTR_DuplicateFrames "\n%d повторних кадрів!\n"
#define MSGTR_SkipFrame "\nКадр пропущено!\n"
#define MSGTR_ResolutionDoesntMatch "\nНовий та попередній відео файл має різне розширення та кольорову гаму.\n"
#define MSGTR_FrameCopyFileMismatch "\nУсі відео файли повинні мати однакові кадр/сек, розширення, та кодек для -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nУсі відео файли повинні мати однакові аудіо кодек та формат для -oac copy.\n"
#define MSGTR_NoAudioFileMismatch "\nНе можу поєднати файли відео з файлами аудіо та відео. Спробуйте -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "ПОПЕРЕДЖЕННЯ: опція -speed не гарантує коректну роботу з -oac copy!\n"\
"Ваше кодування може бути невдалим!\n"
#define MSGTR_ErrorWritingFile "%s: Помилка запису файлу.\n"
#define MSGTR_FlushingVideoFrames "\nЗкидую кадри відео.\n"
#define MSGTR_FiltersHaveNotBeenConfiguredEmptyFile "Фільтри не було налаштовано! Порожній файл?\n"
#define MSGTR_RecommendedVideoBitrate "Рекомендований бітрейт для %s CD: %d\n"
#define MSGTR_EdlSkipStartEndCurrent "EDL SKIP: Початок: %.2f  Кінець: %.2f   Поточна: V: %.2f  A: %.2f     \r"
#define MSGTR_OpenedStream "вдало: формат: %d  дані: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "відеокодек: копія кадрів (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "аудіокодек: копія кадрів (формат=%x каналів=%d швидкість=%d бітів=%d Б/с=%d приклад-%d)\n"
#define MSGTR_CBRPCMAudioSelected "Вибрано CBR PCM аудіо.\n"
#define MSGTR_MP3AudioSelected "Вибрано MP3 аудіо.\n"
#define MSGTR_CannotAllocateBytes "Не можу виділити пам'ять для %d байтів.\n"
#define MSGTR_SettingAudioDelay "Встановлюю аудіо затримку у %5.3fс.\n"
#define MSGTR_SettingVideoDelay "Встановлюю відео затримку у %5.3fс.\n"
#define MSGTR_SettingAudioInputGain "Встановлюю підсилення вхідного сигналу аудіо потоку у %f.\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Обмежити підвантаження аудіо до 0.4с.\n"
#define MSGTR_IncreasingAudioDensity "Збільшую густину аудіо до 4.\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Форсую аудіо підвантаження до 0, максимальну корекцію pts у 0.\n"
#define MSGTR_CBRAudioByterate "\n\nCBR аудіо: %d байтів/сек, %d байтів/блок\n"
#define MSGTR_LameVersion "Версія LAME %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Помилка: Вказаний бітрейт не є вірним для даного встановлення.\n"\
"\n"\
"Використовуючи цей режим ви повинні ввести значення між \"8\" та \"320\".\n"\
"\n"\
"Для подальшої інформації спробуйте: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Помилка: Ви не ввели дійсний профайл та/чи опції з встановлення.\n"\
"\n"\
"Доступні профайли:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Режим ABR) - Мається на увазі режим ABR. Для використання,\n"\
"                       просто вкажіть бітрейт. Наприклад:\n"\
"                       \"preset=185\" активує це\n"\
"                       встановлення та використовує 185 як середнє значення кбіт/с.\n"\
"\n"\
"    Декілька прикладів:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Для подальшої інформації спробуйте: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Встановлення розроблені так, щоб отримати якнайкращу якість.\n"\
"\n"\
"Вони були розроблені та налаштовані у результаті ретельних тестів\n"\
"тести подвійного прослуховування, щоб досягти цього результату.\n"\
"\n"\
"Ключі встановлень постійно поновлюються, щоб відповідати останнім розробленням.\n"\
"в результаті чого ви повинні отримати практично найкращу якість\n"\
"на даний момент можливо при використанні LAME.\n"\
"\n"\
"Щоб активувати ці встановлення:\n"\
"\n"\
"   Для VBR режимів (найкраща якість звичайно):\n"\
"\n"\
"     \"preset=standard\" Звичайно цього встановлення повинно бути достатньо\n"\
"                             для більшості людей та більшості музики, та воно\n"\
"                             являє собою досить високу якість.\n"\
"\n"\
"     \"preset=extreme\" Якщо у вас хороший слух та добра музича апаратура,\n"\
"                             це встановлення як правило забезпечить кращу якість\n"\
"                             ніж режим \"standard\"\n"\
"                             mode.\n"\
"\n"\
"   Для CBR 320kbps (максимально можлива якість, яку можна тримати з встановлень):\n"\
"\n"\
"     \"preset=insane\"  Це встановлення звичайно буде занадто для більшості людей\n"\
"                             та ситуацій, але якщо ви мусите отримати найкращу\n"\
"                             максимально можливу якість, не дивлячись на\n"\
"                             розмір файлу, це ваш вибір.\n"\
"\n"\
"   Для ABR режимів (висока якість для заданого бітрейта, але така висока як VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Використовуючи це встановлення звичайно дає добру якість\n"\
"                             для заданого бітрейта. Базуючись на введеному\n"\
"                             бітрейті, це встановлення визначить оптимальні\n"\
"                             налаштування для кожной конкретного випадку.\n"\
"                             Не дивлячись на то, що цей підхід працює, він\n"\
"                             далеко не такий гнучкий як VBR, та звичайно не досягає\n"\
"                             такого рівня якості як VBR на високих бітрейтах.\n"\
"\n"\
"Наступні опції також доступні для існуючих профілей:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Режим ABR) - Мається на увазі режим ABR. Для використання\n"\
"                       просто вкажіть бітрейт. Наприклад:\n"\
"                       \"preset=185\" активує це встановлення\n"\
"                       та використая 185 як середнє значення кбіт/сек.\n"\
"\n"\
"   \"fast\" - Вмикає новий швидкий VBR для конкретного профілю.\n"\
"            Недостатком цього ключа є те, що часто бітрейт буде\n"\
"            набагато більше ніж у нормальному режимі;\n"\
"            а якість може буте дещо гірше.\n"\
"Попередження: У теперешній версії швидкі встановлення можуть привести до\n"\
"              високому бітрейту, у порівнянні з звичайними встановленнями.\n"\
"\n"\
"   \"cbr\"  - Якщо ви використовуєте режим ABR (див. вище) з бітрейтом кратним\n"\
"            80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            ви можете застосувати опцію \"cbr\" щоб форсувати кодування у режимі\n"\
"            CBR замість стандартного режиму abr. ABR забезпечує кращу якість,\n"\
"            але CBR може бути корисним у таких ситуаціях,\n"\
"            як передача потоків mp3 через інтернет.\n"\
"\n"\
"    Наприклад:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Декілька псевдонімів доступні для режима ABR:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit \
"Не можу встановити опції LAME, перевірте бітрейт/частому_дискретизації, деякі\n"\
"дуже низькі бітрейти (<32) потребують менші частоти\nдискретизації(наприклад, -srate 8000).\n"\
"Якщо все це не допоможе, спробуйте встановлення."
#define MSGTR_ConfigFileError "помилка у файлі налаштувань"
#define MSGTR_ErrorParsingCommandLine "помилка аналізу командного рядка"
#define MSGTR_VideoStreamRequired "Вивід відео обов'язковий!\n"
#define MSGTR_ForcingInputFPS "Вхідні кадри/сек будуть замінені на %5.3f.\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Вихідний формат файлу RAWVIDEO не підтримує аудіо - вимикаю відео.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Цей демультиплексор поки не підтримується -nosound.\n"
#define MSGTR_MemAllocFailed "Не можу виділити пам'ять.\n"
#define MSGTR_NoMatchingFilter "Не можу знайти потрібний фільтр/формат аудіовиводу!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, можливо зламаний компілятор C?\n"
#define MSGTR_NoLavcAudioCodecName "Аудіо LAVC, відсутнє назва кодека!\n"
#define MSGTR_LavcAudioCodecNotFound "Аудіо LAVC, не можу знайти кодувальщик для кодека %s.\n"
#define MSGTR_CouldntAllocateLavcContext "Аудіо LAVC, не можу розмістити контекст!\n"
#define MSGTR_CouldntOpenCodec "Не можу відкрити кодек %s, br=%d.\n"
#define MSGTR_CantCopyAudioFormat "Аудіо формат 0x%x не використовується з '-oac copy', спробуйте\n'-oac pcm' замість чи використайте '-fafmttag' для його перевизначення.\n"

#define MSGTR_VideoStreamResult "\nВідео потік: %8.3f кбіт/с  (%d Б/с)  розмір: %"PRIu64" байт  %5.3f секунд  %d кадрів\n"
#define MSGTR_AudioStreamResult "\nАудіо потік: %8.3f кбіт/с  (%d Б/с)  розмір: %"PRIu64" байт  %5.3f секунд\n"

// cfg-mencoder.h:
#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     метод змінного бітрейту\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           приблизний бітрейт\n"\
"\n"\
" cbr           постійний бітрейт\n"\
"               Forces also CBR mode encoding on subsequent ABR presets modes\n"\
"\n"\
" br=<0-1024>   вказати бітрейт в kBit (тільки для CBR та ABR)\n"\
"\n"\
" q=<0-9>       якість (0-найвища, 9-найнижча) (тільки для VBR)\n"\
"\n"\
" aq=<0-9>      алгорітмична якість (0-краща/повільніша 9-гірша/швидкіша)\n"\
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
" fast          переходити на швидке кодування при послідовних VBR presets modes,\n"\
"               трохи менша якість та більші бітрейти.\n"\
"\n"\
" preset=<value> запровадити найбільші установки якості.\n"\
"                 середня:    VBR кодування, добра якість\n"\
"                 (150-180 kbps бітрейт)\n"\
"                 стандарт:   VBR кодування, висока якість\n"\
"                 (170-210 kbps бітрейт)\n"\
"                 висока:     VBR кодування, дуже висока якість\n"\
"                 (200-240 kbps бітрейт)\n"\
"                 божевільна: CBR кодування, найвища настройка якості\n"\
"                 (320 kbps бітрейт)\n"\
"                 <8-320>:    ABR кодування з вказаним приблизним бітрейтом.\n\n"
   
// open.c, stream.c:
#define MSGTR_CdDevNotfound "Компактовід \"%s\" не знайдений!\n"
#define MSGTR_ErrTrackSelect "Помилка вибору треку на VCD!"
#define MSGTR_ReadSTDIN "Читання з stdin...\n"
#define MSGTR_UnableOpenURL "Не можу відкрити URL: %s\n"
#define MSGTR_ConnToServer "З'єднання з сервером: %s\n"
#define MSGTR_FileNotFound "Файл не знайдений: '%s'\n"

#define MSGTR_SMBFileNotFound "Помилка відкриття з мережі: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer не має вкомпільованої підтримки SMB\n"

#define MSGTR_CantOpenDVD "Не зміг відкрити DVD: %s (%s)\n"
#define MSGTR_DVDnumTitles "Є %d доріжок з титрами на цьому DVD.\n"
#define MSGTR_DVDinvalidTitle "Неприпустимий номер доріжки титрів на DVD: %d\n"
#define MSGTR_DVDnumChapters "Є %d розділів на цій доріжці з DVD титрами.\n"
#define MSGTR_DVDinvalidChapter "Неприпустимий номер DVD розділу: %d\n"
#define MSGTR_DVDnumAngles "Є %d кутів на цій доріжці з DVD титрами.\n"
#define MSGTR_DVDinvalidAngle "Неприпустимий номер DVD кута: %d\n"
#define MSGTR_DVDnoIFO "Не можу відкрити IFO файл для DVD титрів %d.\n"
#define MSGTR_DVDnoVOBs "Не можу відкрити титри VOBS (VTS_%02d_1.VOB).\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Попередження! Заголовок аудіо потоку %d перевизначений!\n"
#define MSGTR_VideoStreamRedefined "Попередження! Заголовок відео потоку %d перевизначений!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Надто багато (%d, %d байтів) аудіо пакетів у буфері!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Надто багато (%d, %d байтів) відео пакетів у буфері!\n"
#define MSGTR_MaybeNI "(можливо ви програєте неперемежений потік/файл або невдалий кодек)\n"
#define MSGTR_SwitchToNi "\nДетектовано погано перемежений AVI файл - переходжу в -ni режим...\n"
#define MSGTR_Detected_XXX_FileFormat "Знайдений %s формат файлу!\n"
#define MSGTR_DetectedAudiofile "Аудіо файл детектовано.\n"
#define MSGTR_NotSystemStream "Не в форматі MPEG System Stream... (можливо, Transport Stream?)\n"
#define MSGTR_FormatNotRecognized "========= Вибачте, формат цього файлу не розпізнаний чи не підтримується ===========\n"\
				  "===== Якщо це AVI, ASF або MPEG потік, будь ласка зв'яжіться з автором! ======\n"
#define MSGTR_MissingVideoStream "Відео потік не знайдений!\n"
#define MSGTR_MissingAudioStream "Аудіо потік не знайдений...  -> програю без звуку\n"
#define MSGTR_MissingVideoStreamBug "Відео потік загублений!? Зв'яжіться з автором, це мабуть помилка :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: файл не містить обраний аудіо або відео потік\n"

#define MSGTR_NI_Forced "Примусово вибраний"
#define MSGTR_NI_Detected "Знайдений"
#define MSGTR_NI_Message "%s НЕПЕРЕМЕЖЕНИЙ формат AVI файлу!\n"

#define MSGTR_UsingNINI "Використання НЕПЕРЕМЕЖЕНОГО або пошкодженого формату AVI файлу!\n"
#define MSGTR_CouldntDetFNo "Не зміг визначити число кадрів (для абсолютного перенесення)\n"
#define MSGTR_CantSeekRawAVI "Не можу переміститися у непроіндексованому потоці .AVI! (вимагається індекс, спробуйте з ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не можу переміщуватися у цьому файлі!\n"

#define MSGTR_MOVcomprhdr "MOV: Стиснуті заголовки (поки що) не підтримуються!\n"
#define MSGTR_MOVvariableFourCC "MOV: Попередження! Знайдено перемінний FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Попередження! надто багато треків!"
#define MSGTR_FoundAudioStream "==> Знайдено аудіо потік: %d\n"
#define MSGTR_FoundVideoStream "==> Знайдено відео потік: %d\n"
#define MSGTR_DetectedTV "Детектовано ТВ! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Неможливо відкрити ogg demuxer.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Пошук аудіо потоку (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Неможливо відкрити аудіо потік: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Неможливо відкрити потік субтитрів: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Не вдалося відкрити аудіо demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Не вдалося відкрити demuxer субтитрів: %s\n"
#define MSGTR_TVInputNotSeekable "TV input is not seekable! (Seeking will probably be for changing channels ;)\n"
#define MSGTR_ClipInfo "Інформація кліпу:\n"


// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не зміг відкрити кодек\n"
#define MSGTR_CantCloseCodec "Не зміг закрити кодек\n"

#define MSGTR_MissingDLLcodec "ПОМИЛКА: Не зміг відкрити необхідний DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не зміг завантажити чи ініціалізувати Win32/ACM AUDIO кодек (загублений DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не можу знайти кодек \"%s\" у libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: КІНЕЦЬ ФАЙЛУ при пошуку послідовності заголовків\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не можу читати послідовність заголовків!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читати розширення послідовності заголовків!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Погана послідовність заголовків!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Погане розширення послідовності заголовків!\n"

#define MSGTR_ShMemAllocFail "Не можу захопити загальну пам'ять\n"
#define MSGTR_CantAllocAudioBuf "Не можу захопити вихідний буфер аудіо\n"

#define MSGTR_UnknownAudio "Невідомий чи загублений аудіо формат, програю без звуку\n"

#define MSGTR_UsingExternalPP "[PP] Використовую зовнішній фільтр обробки, макс q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Використовую обробку кодека, макс q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Відео атрибут '%s' не підтримується вибраними vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Запрошений драйвер відео кодеку [%s] (vfm=%s) недосяжний (ввімкніть його під час компіляції)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Запрошений драйвер аудіо кодеку [%s] (afm=%s) недосяжний (ввімкніть його під час компіляції)\n"
#define MSGTR_OpeningVideoDecoder "Відкриваю відео декодер: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Відкриваю аудіо декодер: [%s] %s\n"
#define MSGTR_UninitVideoStr "відновлення відео: %s\n"
#define MSGTR_UninitAudioStr "відновлення аудіо: %s\n"
#define MSGTR_VDecoderInitFailed "Збій ініціалізації VDecoder :(\n"
#define MSGTR_ADecoderInitFailed "Збій ініціалізації ADecoder :(\n"
#define MSGTR_ADecoderPreinitFailed "Збій підготування ADecoder :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Розподіляю %d байт вхідному буферу\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Розподіляю %d + %d = %d байт вихідному буферу\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Встановлення підтримки lirc...\n"
#define MSGTR_LIRCopenfailed "Невдале відкриття підтримки lirc!\n"
#define MSGTR_LIRCcfgerr "Невдале читання файлу конфігурації LIRC %s!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Неможливо знайти відео фільтр '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Неможливо відкрити відео фільтр '%s'\n"
#define MSGTR_OpeningVideoFilter "Відкриваю відео фільтр: "
//-----------------------------
#define MSGTR_CannotFindColorspace "Не можу підібрати загальну схему кольорів, навіть додавши 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Кодек не встановив sh->disp_w та sh->disp_h, спробую обійти це.\n"
#define MSGTR_VoConfigRequest "VDec: vo config запит - %d x %d (preferred csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Не можу підібрати підходящу схему кольорів - повтор з -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Відношення сторін %.2f:1 - масштабую аби скоректувати.\n"
#define MSGTR_MovieAspectUndefined "Відношення сторін не вказано - масштабування не використовується.\n"

// ====================== GUI messages/buttons ========================

#ifdef CONFIG_GUI

// --- labels ---
#define MSGTR_About "Про програму"
#define MSGTR_FileSelect "Вибрати файл..."
#define MSGTR_SubtitleSelect "Вибрати субтитри..."
#define MSGTR_OtherSelect "Вибір..."
#define MSGTR_AudioFileSelect "Вибрати зовнішній аудіо канал..."
#define MSGTR_FontSelect "Вибрати шрифт..."
#define MSGTR_PlayList "Список програвання"
#define MSGTR_Equalizer "Еквалайзер"
#define MSGTR_SkinBrowser "Переглядач жупанів"
#define MSGTR_Network "Програвання з мережі..."
#define MSGTR_Preferences "Налаштування"
#define MSGTR_NoMediaOpened "Немає відкритого носію."
#define MSGTR_VCDTrack "Доріжка VCD %d"
#define MSGTR_NoChapter "No chapter"
#define MSGTR_Chapter "Chapter %d"
#define MSGTR_NoFileLoaded "Немає завантаженого файлу."

// --- buttons ---
#define MSGTR_Ok "Так"
#define MSGTR_Cancel "Скасувати"
#define MSGTR_Add "Додати"
#define MSGTR_Remove "Видалити"
#define MSGTR_Clear "Вичистити"
#define MSGTR_Config "Конфігурувати"
#define MSGTR_ConfigDriver "Конфігурувати драйвер"
#define MSGTR_Browse "Проглядати"

// --- error messages ---
#define MSGTR_NEMDB "Вибачте, не вистачає пам'яті для відмальовування буферу."
#define MSGTR_NEMFMR "Вибачте, не вистачає пам'яті для відображення меню."
#define MSGTR_IDFGCVD "Вибачте, не знайдено відповідного до GUI вихідного відео драйверу."
#define MSGTR_NEEDLAVC "Вибачте, ви не можете грати не-MPEG файли на вашому DXR3/H+ пристрої без перекодування.\nБудь ласка, ввімкніть lavc в панелі конфігурування DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[жупан] помилка у файлі конфігурації жупана, рядок %d  : %s" 
#define MSGTR_SKIN_WARNING1 "[жупан] попередження: у файлі конфігурації жупана, рядок %d: widget знайдений але до цього не знайдено \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[жупан] попередження: у файлі конфігурації жупана, рядок %d: widget знайдений але до цього не знайдено \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[жупан] попередження: у файлі конфігурації жупана, рядок %d: цей widget (%s) не підтримує цю subsection"
#define MSGTR_SKIN_BITMAP_16bit  "Глибина кольору бітової карти у 16 біт і менше не підтримується (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "файл не знайдений (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "помилка читання BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "помилка читання TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "помилка читання PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE запакований TGA не підтримується (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "невідомий тип файлу (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "помилка перетворення 24-біт у 32-біт (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "невідоме повідомлення: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "не вистачає пам'яті\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "оголошено надто багато шрифтів\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "файл шрифту не знайдений\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "файл образів шрифту не знайдений\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "неіснуючий ідентифікатор шрифту (%s)\n"
#define MSGTR_SKIN_UnknownParameter "невідомий параметр (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Жупан не знайдено (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Помилка читання файла конфігурації жупана (%s).\n"
#define MSGTR_SKIN_LABEL "Жупани:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Про програму"
#define MSGTR_MENU_Open "Відкрити..."
#define MSGTR_MENU_PlayFile "Грати файл..."
#define MSGTR_MENU_PlayVCD "Грати VCD..."
#define MSGTR_MENU_PlayDVD "Грати DVD..."
#define MSGTR_MENU_PlayURL "Грати URL..."
#define MSGTR_MENU_LoadSubtitle "Завантажити субтитри..."
#define MSGTR_MENU_DropSubtitle "Викинути субтитри..."
#define MSGTR_MENU_LoadExternAudioFile "Завантажити зовнішній аудіо файл..."
#define MSGTR_MENU_Playing "Відтворення"
#define MSGTR_MENU_Play "Грати"
#define MSGTR_MENU_Pause "Пауза"
#define MSGTR_MENU_Stop "Зупинити"
#define MSGTR_MENU_NextStream "Наступний потік"
#define MSGTR_MENU_PrevStream "Попередній потік"
#define MSGTR_MENU_Size "Розмір"
#define MSGTR_MENU_NormalSize "Нормальний розмір"
#define MSGTR_MENU_DoubleSize "Подвійний розмір"
#define MSGTR_MENU_FullScreen "Повний екран"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Грати диск..."
#define MSGTR_MENU_ShowDVDMenu "Показати DVD меню"
#define MSGTR_MENU_Titles "Титри"
#define MSGTR_MENU_Title "Титр %2d"
#define MSGTR_MENU_None "(нема)"
#define MSGTR_MENU_Chapters "Розділи"
#define MSGTR_MENU_Chapter "Розділ %2d"
#define MSGTR_MENU_AudioLanguages "Авто мова"
#define MSGTR_MENU_SubtitleLanguages "Мова субтитрів"
#define MSGTR_MENU_SkinBrowser "Переглядач жупанів"
#define MSGTR_MENU_Exit "Вихід..."
#define MSGTR_MENU_Mute "Тиша"
#define MSGTR_MENU_Original "Вихідний"
#define MSGTR_MENU_AspectRatio "Відношення сторін"
#define MSGTR_MENU_AudioTrack "Аудіо доріжка"
#define MSGTR_MENU_Track "Доріжка %d"
#define MSGTR_MENU_VideoTrack "Відео доріжка"

// --- equalizer
#define MSGTR_EQU_Audio "Аудіо"
#define MSGTR_EQU_Video "Відео"
#define MSGTR_EQU_Contrast "Контраст: "
#define MSGTR_EQU_Brightness "Яскравість: "
#define MSGTR_EQU_Hue "Тон: "
#define MSGTR_EQU_Saturation "Насиченість: "
#define MSGTR_EQU_Front_Left "Передній Лівий"
#define MSGTR_EQU_Front_Right "Передній Правий"
#define MSGTR_EQU_Back_Left "Задній Лівий"
#define MSGTR_EQU_Back_Right "Задній Правий"
#define MSGTR_EQU_Center "Центральний"
#define MSGTR_EQU_Bass "Бас"
#define MSGTR_EQU_All "Усі"
#define MSGTR_EQU_Channel1 "Канал 1:"
#define MSGTR_EQU_Channel2 "Канал 2:"
#define MSGTR_EQU_Channel3 "Канал 3:"
#define MSGTR_EQU_Channel4 "Канал 4:"
#define MSGTR_EQU_Channel5 "Канал 5:"
#define MSGTR_EQU_Channel6 "Канал 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Шлях"
#define MSGTR_PLAYLIST_Selected "Вибрані файли"
#define MSGTR_PLAYLIST_Files "Файли"
#define MSGTR_PLAYLIST_DirectoryTree "Дерево каталогу"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Субтитри й OSD"
#define MSGTR_PREFERENCES_Codecs "Кодеки й demuxer"
#define MSGTR_PREFERENCES_Misc "Різне"

#define MSGTR_PREFERENCES_None "Немає"
#define MSGTR_PREFERENCES_AvailableDrivers "Доступні драйвери:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Не грати звук"
#define MSGTR_PREFERENCES_NormalizeSound "Нормалізувати звук"
#define MSGTR_PREFERENCES_EnableEqualizer "Дозволити еквалайзер"
#define MSGTR_PREFERENCES_ExtraStereo "Дозволити додаткове стерео"
#define MSGTR_PREFERENCES_Coefficient "Коефіціент:"
#define MSGTR_PREFERENCES_AudioDelay "Затримка аудіо"
#define MSGTR_PREFERENCES_DoubleBuffer "Дозволити подвійне буферування"
#define MSGTR_PREFERENCES_DirectRender "Дозволити прямий вивід"
#define MSGTR_PREFERENCES_FrameDrop "Дозволити пропуск кадрів"
#define MSGTR_PREFERENCES_HFrameDrop "Дозволити викидування кадрів (небезпечно)"
#define MSGTR_PREFERENCES_Flip "Перегорнути зображення догори ногами"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Таймер та індікатори"
#define MSGTR_PREFERENCES_OSDProgress "Лише лінійки"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Таймер, проценти та загальний час"
#define MSGTR_PREFERENCES_Subtitle "Субтитри:"
#define MSGTR_PREFERENCES_SUB_Delay "Затримка: "
#define MSGTR_PREFERENCES_SUB_FPS "к/c:"
#define MSGTR_PREFERENCES_SUB_POS "Положення: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Заборонити автозавантаження субтитрів"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode субтитри"
#define MSGTR_PREFERENCES_SUB_MPSUB "Перетворити вказані субтитри до формату MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Перетворити вказані субтитри до формату SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Дозволити/заборонити перекриття субтитрів"
#define MSGTR_PREFERENCES_Font "Шрифт:"
#define MSGTR_PREFERENCES_FontFactor "Фактор шрифту:"
#define MSGTR_PREFERENCES_PostProcess "Дозволити postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Авто якість: "
#define MSGTR_PREFERENCES_NI "Використовувати неперемежений AVI парсер"
#define MSGTR_PREFERENCES_IDX "Перебудувати індекс, якщо треба"
#define MSGTR_PREFERENCES_VideoCodecFamily "Драйвер відео содеку:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Драйвер аудіо кодеку:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Рівень OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Субтитри"
#define MSGTR_PREFERENCES_FRAME_Font "Шрифт"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Кодек й demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Кеш"
#define MSGTR_PREFERENCES_Message "Не забудьте, що вам треба перезапустити програвання для набуття чинності деяких параметрів!"
#define MSGTR_PREFERENCES_DXR3_VENC "Відео кодек:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Використовувати LAVC (FFmpeg)"
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
#define MSGTR_PREFERENCES_FontPropWidth "Пропорційно ширині кадру"
#define MSGTR_PREFERENCES_FontPropHeight "Пропорційно висоті кадру"
#define MSGTR_PREFERENCES_FontPropDiagonal "Пропорційно діагоналі кадру"
#define MSGTR_PREFERENCES_FontEncoding "Кодування:"
#define MSGTR_PREFERENCES_FontBlur "Розпливання:"
#define MSGTR_PREFERENCES_FontOutLine "Обведення:"
#define MSGTR_PREFERENCES_FontTextScale "Масштаб тексту:"
#define MSGTR_PREFERENCES_FontOSDScale "Масштаб OSD:"
#define MSGTR_PREFERENCES_Cache "Кеш on/off"
#define MSGTR_PREFERENCES_CacheSize "Розмір кешу: "
#define MSGTR_PREFERENCES_LoadFullscreen "Стартувати в полний екран"
#define MSGTR_PREFERENCES_SaveWinPos "Зберігати положення вікна"
#define MSGTR_PREFERENCES_XSCREENSAVER "Stop XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Дозволити лінійку програвання"
#define MSGTR_PREFERENCES_AutoSync "AutoSync on/off"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM пристрій:"
#define MSGTR_PREFERENCES_DVDDevice "DVD пристрій:"
#define MSGTR_PREFERENCES_FPS "Кадрів на секунду:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Показувати неактивне вікно зображення"

#define MSGTR_ABOUT_UHU "GUI розробку спонсовано UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "фатальна помилка..."
#define MSGTR_MSGBOX_LABEL_Error "помилка..."
#define MSGTR_MSGBOX_LABEL_Warning "попередження..." 

#endif
