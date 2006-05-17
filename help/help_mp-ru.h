/* Translated by:  Nick Kurshev <nickols_k@mail.ru>,
 *		Dmitry Baryshkov <mitya@school.ioffe.ru>

   Reworked by Savchenko Andrew aka Bircoph <Bircoph[at]list[dot]ru>
   Was synced with help_mp-en.h: rev 1.157
 ========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Запуск:   mplayer [опции] [URL|путь/]имя_файла\n"
"\n"
"Базовые опции: (полный список см. на man-странице)\n"
" -vo <drv[:dev]> выбор драйвера и устройства видео вывода (список см. с '-vo help')\n"
" -ao <drv[:dev]> выбор драйвера и устройства аудио вывода (список см. с '-ao help')\n"
#ifdef HAVE_VCD
" vcd://<номер трека> играть дорожку (S)VCD (Super Video CD) (указывайте устройство,\n                 не монтируйте его)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<номер ролика> играть DVD ролик с устройства вместо файла\n"
" -alang/-slang   выбрать язык аудио/субтитров DVD (двубуквенный код страны)\n"
#endif
" -ss <время>     переместиться на заданную (секунды или ЧЧ:ММ:СС) позицию\n"
" -nosound        без звука\n"
" -fs             опции полноэкранного проигрывания (или -vm, -zoom, подробности на man-странице)\n"
" -x <x> -y <y>   установить разрешение дисплея (использовать с -vm или -zoom)\n"
" -sub <файл>     указать файл субтитров (см. также -subfps, -subdelay)\n"
" -playlist <файл> указать список воспроизведения (playlist)\n"
" -vid x -aid y   опции для выбора видео (x) и аудио (y) потока для проигрывания\n"
" -fps x -srate y опции для изменения видео (x кадр/сек) и аудио (y Гц) скорости\n"
" -pp <quality>   разрешить фильтр постобработки (подробности на man-странице)\n"
" -framedrop      включить отбрасывание кадров (для медленных машин)\n"
"\n"
"Основные кнопки: (полный список в странице man, также смотри input.conf)\n"
" <- или ->       перемещение вперёд/назад на 10 секунд\n"
" up или down     перемещение вперёд/назад на  1 минуту\n"
" pgup or pgdown  перемещение вперёд/назад на 10 минут\n"
" < или >         перемещение вперёд/назад в списке воспроизведения (playlist'е)\n"
" p или ПРОБЕЛ    приостановить фильм (любая клавиша - продолжить)\n"
" q или ESC       остановить воспроизведение и выйти\n"
" + или -         регулировать задержку звука по +/- 0.1 секунде\n"
" o               цикличный перебор OSD режимов:  нет / навигация / навигация+таймер\n"
" * или /         прибавить или убавить PCM громкость\n"
" z или x         регулировать задержку субтитров по +/- 0.1 секунде\n"
" r или t         регулировка вертикальной позиции субтитров, см. также -vf expand\n"
"\n"
" * * * ПОДРОБНЕЕ СМ. ДОКУМЕНТАЦИЮ, О ДОПОЛНИТЕЛЬНЫХ ОПЦИЯХ И КЛЮЧАХ! * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Для улучшения поддержки необходимы образцы этого формата.\nПожалуйста, свяжитесь с разработчиками.\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nВыходим...\n"
#define MSGTR_ExitingHow "\nВыходим... (%s)\n"
#define MSGTR_Exit_quit "Выход"
#define MSGTR_Exit_eof "Конец файла"
#define MSGTR_Exit_error "Фатальная ошибка"
#define MSGTR_IntBySignal "\nMPlayer прерван сигналом %d в модуле: %s \n"
#define MSGTR_NoHomeDir "Не могу найти HOME(домашний) каталог\n"
#define MSGTR_GetpathProblem "проблемы в get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Создание файла конфигурации: %s\n"
#define MSGTR_CopyCodecsConf "(скопируйте/создайте_ссылку etc/codecs.conf (из исходников MPlayer) в ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Используется встроенный codecs.conf.\n"
#define MSGTR_CantLoadFont "Не могу загрузить шрифт: %s\n"
#define MSGTR_CantLoadSub "Не могу загрузить субтитры: %s\n"
#define MSGTR_DumpSelectedStreamMissing "дамп: ФАТАЛЬНАЯ ОШИБКА: Выбранный поток потерян!\n"
#define MSGTR_CantOpenDumpfile "Не могу открыть файл для дампирования!!!\n"
#define MSGTR_CoreDumped "Создан дамп ядра ;)\n"
#define MSGTR_FPSnotspecified "В заголовке кадры/сек не указаны (или недопустимые)! Используйте -fps опцию!\n"
#define MSGTR_TryForceAudioFmtStr "Попытка форсировать семейство аудио кодеков %s...\n"
#define MSGTR_CantFindAudioCodec "Не могу найти кодек для аудио формата 0x%X!\n"
#define MSGTR_RTFMCodecs "Прочтите DOCS/HTML/ru/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Попытка форсировать семейство видео кодеков %s...\n"
#define MSGTR_CantFindVideoCodec "Не могу найти кодек для выбранного -vo и видео формата 0x%X!\n"
#define MSGTR_CannotInitVO "ФАТАЛЬНАЯ ОШИБКА: Не могу инициализировать видео драйвер!\n"
#define MSGTR_CannotInitAO "не могу открыть/инициализировать аудио устройство -> БЕЗ ЗВУКА\n"
#define MSGTR_StartPlaying "Начало воcпроизведения...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         *****************************************************************\n"\
"         **** Ваша система слишком МЕДЛЕННА чтобы воспроизводить это! ****\n"\
"         *****************************************************************\n"\
"Возможные причины, проблемы, обходы: \n"\
"- Наиболее частая: плохой/сырой _аудио_ драйвер\n"\
"  - Попробуйте -ao sdl или используйте эмуляцию OSS на ALSA.\n"\
"  - Поэкспериментируйте с различными значениями -autosync, начните с 30.\n"\
"- Медленный видео вывод\n"\
"  - Попытайтесь другие -vo driver (список: -vo help) или попытайтесь с -framedrop!\n"\
"- Медленный процессор\n"\
"  - Не пытайтесь воспроизводить большие DVD/DivX на медленных процессорах!\n  попытайтесь -hardframedrop\n"\
"- Битый файл.\n"\
"  - Попытайтесь различные комбинации: -nobps  -ni  -mc 0  -forceidx\n"\
"- Медленный носитель (смонтированные NFS/SMB, DVD, VCD и т.п.)\n"\
"  - Используйте -cache 8192.\n"\
"- Используете ли Вы -cache для проигрывания не-'слоёных'[non-interleaved] AVI файлов?\n"\
"  - Используйте -nocache.\n"\
"Читайте DOCS/HTML/ru/devices.html для советов по подстройке/ускорению.\n"\
"Если ничего не помогло, тогда читайте DOCS/HTML/ru/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer был скомпилирован БЕЗ поддержки GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI требует X11!\n"
#define MSGTR_Playing "Проигрывание %s.\n"
#define MSGTR_NoSound "Аудио: без звука!!!\n"
#define MSGTR_FPSforced "Кадры/сек форсированы в %5.3f (время кадра: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Скомпилировано для определения типа процессора во время выполнения - ПРЕДУПРЕЖДЕНИЕ - это \nне оптимально! Для получения максимальной производительности, перекомпилируйте MPlayer\nc --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Скомпилировано для x86 CPU со следующими расширениями:"
#define MSGTR_AvailableVideoOutputDrivers "Доступные драйвера вывода видео:\n"
#define MSGTR_AvailableAudioOutputDrivers "Доступные драйвера вывода звука:\n"
#define MSGTR_AvailableAudioCodecs "Доступные аудио кодеки:\n"
#define MSGTR_AvailableVideoCodecs "Доступные видео кодеки:\n"
#define MSGTR_AvailableAudioFm "Доступные (скомпилированные) семейства/драйверы аудио кодеков:\n"
#define MSGTR_AvailableVideoFm "Доступные (скомпилированные) семейства/драйверы видео кодеков:\n"
#define MSGTR_AvailableFsType "Доступные режимы изменения полноэкранного слоя:\n"
#define MSGTR_UsingRTCTiming "Используется аппаратная Linux RTC синхронизация (%ldГц).\n"
#define MSGTR_CannotReadVideoProperties "Видео: Не могу прочитать свойства.\n"
#define MSGTR_NoStreamFound "Поток не найден.\n"
#define MSGTR_ErrorInitializingVODevice "Ошибка при открытии/инициализации выбранного устройства видео вывода (-vo).\n"
#define MSGTR_ForcedVideoCodec "Форсирован видео кодек: %s\n"
#define MSGTR_ForcedAudioCodec "Форсирован аудио кодек: %s\n"
#define MSGTR_Video_NoVideo "Видео: нет видео\n"
#define MSGTR_NotInitializeVOPorVO "\nФАТАЛЬНАЯ ОШИБКА: Не могу инициализировать видео фильтры (-vf) или видео вывод (-vo).\n"
#define MSGTR_Paused "\n====ПРИОСТАНОВЛЕНО====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nНе могу загрузить список воспроизведения (playlist) %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer сломался из-за 'Неправильной Инструкции'.\n"\
"  Это может быть ошибкой нашего нового кода определения типа CPU во время выполнения...\n"\
"  Пожалуйста, читайте DOCS/HTML/ru/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer сломался из-за 'Неправильной Инструкции'.\n"\
"  Обычно, это происходит когда Вы его запускаете на CPU, отличном от того, для которого\n"\
"  он был скомпилирован/оптимизирован.\n"\
"  Проверьте это!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer сломался из-за плохого использования CPU/FPU/RAM.\n"\
"  Перекомпилируйте MPlayer с --enable-debug и сделайте 'gdb' backtrace и\n"\
"  дизассемблирование. Для подробностей, см. DOCS/HTML/ru/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer сломался. Это не должно происходить.\n"\
"  Это может быть ошибкой в коде MPlayer'а _или_ в Вашем драйвере _или_\n"\
"  Вашей версии gcc. Если Вы думаете, что в этом виноват MPlayer, пожалуйста,\n"\
"  прочтите DOCS/HTML/ru/bugreports.html и следуйте инструкциям оттуда. Мы не сможем\n"\
"  и не будем помогать, если Вы не предоставите эту информацию, сообщая о возможной ошибке.\n"
#define MSGTR_LoadingConfig "Загружаю конфигурационный файл '%s'\n"
#define MSGTR_AddedSubtitleFile "СУБТИТРЫ: добавлен файл субтитров (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Ошибка открытия файла [%s] для записи!\n"
#define MSGTR_CommandLine "Командная строка:"
#define MSGTR_RTCDeviceNotOpenable "Не могу открыть %s: %s (пользователь должен обладать правом чтения на этот файл).\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Ошибка инициализации Linux RTC в ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Попробуйте добавить \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" \nв загрузочные скрипты Вашей системы.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Ошибка инициализации Linux RTC в ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Используется %s синхронизация.\n"
#define MSGTR_MenuInitialized "Меню инициализировано: %s\n"
#define MSGTR_MenuInitFailed "Не могу инициализировать меню.\n"
#define MSGTR_Getch2InitializedTwice "ПРЕДУПРЕЖДЕНИЕ: getch2_init вызван дважды!\n"
#define MSGTR_DumpstreamFdUnavailable "Не могу создать дамп этого потока - нет доступных 'fd' (файловых описателей).\n"
#define MSGTR_FallingBackOnPlaylist "Не могу разобрать синтаксис списка воспроизведения %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Не могу открыть видеофильтр libmenu с этим корневым меню %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Ошибка в цепочке pre-init аудиофильтра!\n"
#define MSGTR_LinuxRTCReadError "Ошибка чтения Linux RTC: %s\n"
#define MSGTR_SoftsleepUnderflow "Предупреждение! Недопустимо низкое значение программной задержки!\n"
#define MSGTR_DvdnavNullEvent "Событие DVDNAV NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "Событие DVDNAV: Событие выделения сломано\n"
#define MSGTR_DvdnavEvent "Событие DVDNAV: %s\n"
#define MSGTR_DvdnavHighlightHide "Событие DVDNAV: Выделение скрыто\n"
#define MSGTR_DvdnavStillFrame "######################################## Событие DVDNAV: Стоп-кадр: %d сек\n"
#define MSGTR_DvdnavNavStop "Событие DVDNAV: Остановка Nav \n"
#define MSGTR_DvdnavNavNOP "Событие DVDNAV: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "Событие DVDNAV: Изменение SPU-потока Nav: физически: %d/%d/%d логически: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "Событие DVDNAV: Изменение SPU-потока Nav: физически: %d логически: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "Событие DVDNAV: Изменение аудиопотока Nav: физически: %d логически: %d\n"
#define MSGTR_DvdnavNavVTSChange "Событие DVDNAV: Изменение Nav VTS\n"
#define MSGTR_DvdnavNavCellChange "Событие DVDNAV: Изменение ячейки Nav\n"
#define MSGTR_DvdnavNavSpuClutChange "Событие DVDNAV: Изменение Nav SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "Событие DVDNAV: Завершено позиционирование Nav\n"
#define MSGTR_MenuCall "Вызов меню\n"
#define MSGTR_EdlOutOfMem "Не могу выделить достаточный объём памяти для хранения данных EDL.\n"
#define MSGTR_EdlRecordsNo "Читение %d EDL действий.\n"
#define MSGTR_EdlQueueEmpty "Нет действий EDL, которые следует исполнить (очередь пуста).\n"
#define MSGTR_EdlCantOpenForWrite "Не могу открыть файл EDL [%s] для записи.\n"
#define MSGTR_EdlCantOpenForRead "Не могу открыть файл EDL [%s] для чтения.\n"
#define MSGTR_EdlNOsh_video "Нельзя использовать EDL без видео, отключаю.\n"
#define MSGTR_EdlNOValidLine "Неверная строка EDL: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Плохо форматированная строка EDL [%d]. Пропускаю.\n"
#define MSGTR_EdlBadLineOverlap "Последняя позиция останова была [%f]; следующая стартовая "\
"позиция [%f]. Записи должны быть в хронологическом порядке, не могу перекрыть. Пропускаю.\n"
#define MSGTR_EdlBadLineBadStop "Время останова должно быть после времени старта.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Использую следующий файл для контроля 3-го прохода: %s\n"
#define MSGTR_MissingFilename "\nПропущено имя файла.\n\n"
#define MSGTR_CannotOpenFile_Device "Не могу открыть файл/устройство.\n"
#define MSGTR_CannotOpenDemuxer "Не могу открыть демуксер [demuxer].\n"
#define MSGTR_NoAudioEncoderSelected "\nКодировщик аудио (-oac) не выбран. Выберете какой-нибудь (см. -oac help) или используйте -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nКодировщик видео (-ovc) не выбран. Выберете какой-нибудь (см. -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Не могу открыть файл вывода '%s'.\n"
#define MSGTR_EncoderOpenFailed "Не могу открыть кодировщик.\n"
#define MSGTR_ForcingOutputFourcc "Выходной fourcc форсирован в %x [%.4s]\n"
#define MSGTR_DuplicateFrames "\n%d повторяющийся(хся) кадр(а/ов)!\n"
#define MSGTR_SkipFrame "\nПропускаю кадр!\n"
#define MSGTR_ErrorWritingFile "%s: Ошибка при записи файла.\n"
#define MSGTR_RecommendedVideoBitrate "Рекомендуемый битпоток для %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nПоток видео: %8.3f кбит/с  (%d B/s)  размер: %"PRIu64" байт(а/ов)  %5.3f сек.  %d кадр(а/ов)\n"
#define MSGTR_AudioStreamResult "\nПоток аудио: %8.3f кбит/с  (%d B/s)  размер: %"PRIu64" байт(а/ов)  %5.3f сек.\n"
#define MSGTR_OpenedStream "успех: формат: %d  данные: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "видеокодек: копирование кадров (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "аудиокодек: копирование кадров (формат=%x цепочек=%d скорость=%d битов=%d B/s=%d образец=%d)\n"
#define MSGTR_CBRPCMAudioSelected "Выбрано CBR PCM аудио\n"
#define MSGTR_MP3AudioSelected "Выбрано MP3 аудио\n"
#define MSGTR_CannotAllocateBytes "Не могу выделить память для %d байт\n"
#define MSGTR_SettingAudioDelay "Устанавливаю АУДИО ЗАДЕРЖКУ в %5.3f\n"
#define MSGTR_SettingAudioInputGain "Устанавливаю усиление входящего аудиопотока в %f\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Ограничиваю предзагрузку аудио до 0.4с\n"
#define MSGTR_IncreasingAudioDensity "Увеличиваю плотность аудио до 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Форсирую предзагрузку аудио в 0, максимальную коррекцию pts в 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR аудио: %d байт/сек, %d байт/блок\n"
#define MSGTR_LameVersion "Версия LAME %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Ошибка: Заданный битпоток вне допустимого значения для данной предустановки\n"\
"\n"\
"При использовании этого режима Вы должны указать значение между \"8\" и \"320\"\n"\
"\n"\
"Для дополнительной информации используйте: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Ошибка: Вы не указали верный профиль и/или опции предустановки\n"\
"\n"\
"Доступные профили:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Режим ABR) - Подразумевается режим ABR. Для использования\n"\
"                       просто укажите битпоток. Например:\n"\
"                       \"preset=185\" активирует эту предустановку (preset)\n"\
"                       и использует 185 как среднее значение кбит/сек.\n"\
"\n"\
"    Несколько примеров:\n"\
"\n"\
"     \"-lameopts fast:preset=standard  \"\n"\
" или \"-lameopts  cbr:preset=192       \"\n"\
" или \"-lameopts      preset=172       \"\n"\
" или \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Для дополнительной информации используйте: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Ключи предустановок разработаны с целью предоставления максимально возможного качества.\n"\
"\n"\
"Они были преимущественно разработаны и настроены с помощью тщательных тестов\n"\
"двойного прослушивания для проверки и достижения этой цели.\n"\
"\n"\
"Ключи предустановок постоянно обновляются для соответсвия последним разработкам,\n"\
"в результате чего Вы должны получить практически наилучшее качество, \n"\
"возможное на текущий момент при использовании LAME.\n"\
"\n"\
"Чтобы использовать эти предустановки:\n"\
"\n"\
"   Для VBR режимов (обычно лучшее качество):\n"\
"\n"\
"     \"preset=standard\" Обычно этой предустановки должно быть достаточно\n"\
"                             для большинства людей и большиства музыки, и она\n"\
"                             уже предоставляет достаточно высокое качество.\n"\
"\n"\
"     \"preset=extreme\" Если Вы обладаете чрезвычайно хорошим слухом и\n"\
"                             соответствующим оборудованием, эта предустановка,\n"\
"                             как правило, предоставит несколько лучшее качество,\n"\
"                             чем режим \"standard\".\n"\
"\n"\
"   Для CBR 320kbps (максимально возможное качество, получаемое\n                             при использовании ключей предустановок):\n"\
"\n"\
"     \"preset=insane\"  Использование этой установки является перебором для\n"\
"                             большинства людей и большинства ситуаций, но если\n"\
"                             Вам необходимо максимально возможное качество,\n"\
"                             невзирая на размер файла - это способ сделать так.\n"\
"\n"\
"   Для ABR режимов (высокое качество для заданного битпотока, но не такое высокое, как VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Исполльзование этой предустановки обычно даёт хорошее\n"\
"                             качество для заданного битпотока. Основываясь на\n"\
"                             введённом битпотоке, эта предустановка определит\n"\
"                             оптимальные настройки для каждой конкретной ситуации.\n"\
"                             Несмотря на то, что этот подход работает, он далеко\n"\
"                             не такой гибкий как VBR и обычно не достигает\n"\
"                             такого же уровня качества как VBR на высоких битпотоках.\n"\
"\n"\
"Также доступны следующие опции для соответсвующих профилей:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Режим ABR) - Подразумевается режим ABR. Для использования\n"\
"                       просто укажите битпоток. Например:\n"\
"                       \"preset=185\" активирует эту предустановку (preset)\n"\
"                       и использует 185 как среднее значение кбит/сек.\n"\
"\n"\
"   \"fast\" - Включает новый быстрый VBR для конкретного профиля.\n"\
"            Недостатком этого ключа является то, что часто\n"\
"            битпоток будет немного больше, чем в нормальном режиме;\n"\
"            также качество может быть несколько хуже.\n"\
"Предупреждение: В текущей версии быстрые предустановки могут привести к слишком\n"\
"                высокому битпотоку, по сравнению с обычными предустановками.\n"\
"\n"\
"   \"cbr\"  - Если Вы используете режим ABR (см. выше) с таким \"кратным\""\
"            битпотоком как 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            Вы можете использовать опцию \"cbr\" для форсирования кодирования\n"\
"            в режиме CBR вместо стандартного abr режима. ABR предоставляет\n"\
"            более высокое качество, но CBR может быть полезным в таких\n"\
"            ситуациях как передача потоков mp3 через интернет.\n"\
"\n"\
"    Например:\n"\
"\n"\
"     \"-lameopts fast:preset=standard  \"\n"\
" или \"-lameopts  cbr:preset=192       \"\n"\
" или \"-lameopts      preset=172       \"\n"\
" или \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Несколько псевдонимов доступно для режима ABR:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_ConfigFileError "ошибка в конфигурационном файле"
#define MSGTR_ErrorParsingCommandLine "ошибка при разборе синтаксиса командной строки"
#define MSGTR_VideoStreamRequired "Наличие видеопотока обязательно!\n"
#define MSGTR_ForcingInputFPS "входные кадры/сек будут заменены на %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Выходной формат файла RAWVIDEO не поддерживает аудио - отключаю аудио\n"
#define MSGTR_DemuxerDoesntSupportNosound "Этот демуксер [demuxer] пока что не поддерживает -nosound.\n"
#define MSGTR_MemAllocFailed "не могу выделить память"
#define MSGTR_NoMatchingFilter "Не могу найти соответствующий фильтр/ao формат!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, возможно, сломанный компилятор C?\n"
#define MSGTR_NoLavcAudioCodecName "Аудио LAVC, пропущено имя кодека!\n"
#define MSGTR_LavcAudioCodecNotFound "Аудио LAVC, не могу найти кодировщик для кодека %s\n"
#define MSGTR_CouldntAllocateLavcContext "Аудио LAVC, не могу разместить контекст!\n"
#define MSGTR_CouldntOpenCodec "Не могу открыть кодек %s, br=%d\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     метод для кодирования с переменным битпотоком\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(по умолчанию)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           усреднённый битпоток\n"\
"\n"\
" cbr           постоянный битпоток\n"\
"               Также форсирует CBR режим кодирования в некоторых предустановленных ABR режимах\n"\
"\n"\
" br=<0-1024>   укажите битпоток в кбит (только CBR и ABR)\n"\
"\n"\
" q=<0-9>       качество (0-высшее, 9-наименьшее) (только для VBR)\n"\
"\n"\
" aq=<0-9>      качество алгоритма (0-лучшее/самый медленный, 9-худшее/быстрейший)\n"\
"\n"\
" ratio=<1-100> коэффициент сжатия\n"\
"\n"\
" vol=<0-10>    установите усиление входящего аудио\n"\
"\n"\
" mode=<0-3>    (по-умолчанию: автоопределение)\n"\
"                0: стерео\n"\
"                1: объединённое стерео [joint-stereo]\n"\
"                2: двухканальный\n"\
"                3: моно\n"\
"\n"\
" padding=<0-2>\n"\
"                0: нет\n"\
"                1: все\n"\
"                2: регулируемое\n"\
"\n"\
" fast          Переключение на быстрое кодирование в некоторых предустановленных VBR\n"\
"               режимах; предоставляет несколько худшее качество и завышенные битпотоки.\n"\
"\n"\
" preset=<value> Предоставляет установки наибольшего возможного качества.\n"\
"                 medium: VBR  кодирование, хорошее качество\n"\
"                 (амплитуда битпотока - 150-180 kbps)\n"\
"                 standard: VBR кодирование, высокое качество\n"\
"                 (амплитуда битпотока - 170-210 kbps)\n"\
"                 extreme: VBR кодирование, очень высокое качество\n"\
"                 (амплитуда битпотока - 200-240 kbps)\n"\
"                 insane:  CBR кодирование, лучшее предустановленное качество\n"\
"                 (битпоток 320 kbps)\n"\
"                 <8-320>: ABR кодирование с заданным в кбит средним битпотоком.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "повторяющийся FourCC"
#define MSGTR_TooManyFourccs "слишком много FourCCs/форматов..."
#define MSGTR_ParseError "ошибка разбора синтаксиса"
#define MSGTR_ParseErrorFIDNotNumber "ошибка разбора синтаксиса (ID формата не число?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "ошибка разбора синтаксиса (псевдоним ID формата не число?)"
#define MSGTR_DuplicateFID "повторяющееся ID формата"
#define MSGTR_TooManyOut "слишком много выходных форматов..."
#define MSGTR_InvalidCodecName "\nимя кодека '%s' не верно!\n"
#define MSGTR_CodecLacksFourcc "\nкодек '%s' не имеет FourCC/формат!\n"
#define MSGTR_CodecLacksDriver "\nкодек '%s' не имеет драйвера!\n"
#define MSGTR_CodecNeedsDLL "\nкодеку '%s' необходима 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nкодеку '%s' необходим 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Не могу выделить память для комментария. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Читаю '%s': "
#define MSGTR_CantOpenFileError "Не могу открыть '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Не могу выделить пямять для строки: %s\n"
#define MSGTR_CantReallocCodecsp "Не могу выполнить realloc для '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Имя кодека '%s' не уникально."
#define MSGTR_CantStrdupName "Не могу выполнить strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Не могу выполнить strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Не могу выполнить strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Не могу выполнить strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d аудио & %d видео кодеков\n"
#define MSGTR_CodecDefinitionIncorrect "Кодек орпеделён некорректно."
#define MSGTR_OutdatedCodecsConf "Этот codecs.conf слишком стар и несовместим с данным релизом MPlayer'а!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "нехватка памяти"
#define MSGTR_OverridingTooLowBitrate "Указанный битпоток слишком низкий для данного клипа.\n"\
"Минимально возможное значение для клипа составляет %.0f кбит/сек. Переопределяю\n"\
"заданное пользователем значение.\n"

// fifo.c
#define MSGTR_CannotMakePipe "Не могу создать канал!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Найден слишком старый слот сохранения с lvl %d: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Опция %s не может использоваться в конфигурационном файле.\n"
#define MSGTR_InvalidCmdlineOption "Опция %s не может использоваться в командной строке.\n"
#define MSGTR_InvalidSuboption "Ошибка: у опции '%s' нет субопции '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Ошибка: у субопции '%s' опции '%s' должен быть параметр!\n"
#define MSGTR_MissingOptionParameter "Ошибка: у опции '%s' должен быть параметр!\n"
#define MSGTR_OptionListHeader "\n Имя                  Тип             Минимум    Максимум Общий   CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nВсего: %d опций(я/и)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM '%s' не найден!\n"
#define MSGTR_ErrTrackSelect "Ошибка выбора дорожки VCD!"
#define MSGTR_ReadSTDIN "Чтение из stdin (со стандартного входа)...\n"
#define MSGTR_UnableOpenURL "Не могу открыть URL: %s\n"
#define MSGTR_ConnToServer "Соединение с сервером: %s\n"
#define MSGTR_FileNotFound "Файл не найден: '%s'\n"

#define MSGTR_SMBInitError "Не могу инициализировать библиотеку libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Не могу открыть по сети: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer не был скомпилирован с поддержкой чтения SMB.\n"

#define MSGTR_CantOpenDVD "Не могу открыть DVD: %s\n"
#define MSGTR_DVDwait "Чтение структуры диска, подождите, пожалуйста...\n"
#define MSGTR_DVDnumTitles "На этом DVD %d роликов.\n"
#define MSGTR_DVDinvalidTitle "Недопустимый номер DVD ролика: %d\n"
#define MSGTR_DVDnumChapters "В этом DVD ролике %d раздел[а/ов].\n"
#define MSGTR_DVDinvalidChapter "Недопустимый номер DVD главы: %d\n"
#define MSGTR_DVDnumAngles "В этом DVD ролике %d углов.\n"
#define MSGTR_DVDinvalidAngle "Недопустимый номер DVD угла: %d\n"
#define MSGTR_DVDnoIFO "Не могу открыть IFO файл для DVD ролика %d.\n"
#define MSGTR_DVDnoVOBs "Не могу открыть VOBS ролика (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD успешно открыт!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "ПРЕДУПРЕЖДЕНИЕ: Заголовок аудио потока %d переопределён!\n"
#define MSGTR_VideoStreamRedefined "ПРЕДУПРЕЖДЕНИЕ: Заголовок видео потока %d переопределён!\n"
#define MSGTR_TooManyAudioInBuffer "\nСлишком много (%d в %d байтах) аудио пакетов в буфере!\n"
#define MSGTR_TooManyVideoInBuffer "\nСлишком много (%d в %d байтах) видео пакетов в буфере!\n"
#define MSGTR_MaybeNI "Возможно Вы проигрываете 'неслоёный' поток/файл или неудачный кодек?\n" \
                     "Для AVI файлов попробуйте форсировать 'неслоёный' режим опцией -ni.\n"
#define MSGTR_SwitchToNi "\nОбнаружен плохо 'слоёный' AVI файл - переключаюсь в -ni режим...\n"
#define MSGTR_Detected_XXX_FileFormat "Обнаружен %s формат файла!\n"
#define MSGTR_DetectedAudiofile "Обнаружен аудио файл.\n"
#define MSGTR_NotSystemStream "Не MPEG System Stream формат... (возможно Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Недопустимый MPEG-ES поток??? свяжитесь с автором, это может быть багом :(\n"
#define MSGTR_FormatNotRecognized "======= Извините, формат этого файла не распознан/не поддерживается ==========\n"\
				  "===== Если это AVI, ASF или MPEG поток, пожалуйста свяжитесь с автором! ======\n"
#define MSGTR_MissingVideoStream "Видео поток не найден!\n"
#define MSGTR_MissingAudioStream "Аудио поток не найден -> без звука\n"
#define MSGTR_MissingVideoStreamBug "Видео поток потерян!? свяжитесь с автором, это может быть багом :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: в файле нет выбранного аудио или видео потока\n"

#define MSGTR_NI_Forced "Форсирован"
#define MSGTR_NI_Detected "Обнаружен"
#define MSGTR_NI_Message "%s 'НЕСЛОЁНЫЙ' формат AVI файла!\n"

#define MSGTR_UsingNINI "Использование 'НЕСЛОЁНОГО' испорченного формата AVI файла!\n"
#define MSGTR_CouldntDetFNo "Не смог определить число кадров (для абсолютного перемещения).\n"
#define MSGTR_CantSeekRawAVI "Не могу переместиться в сыром потоке AVI! (требуется индекс, попробуйте с ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не могу перемещаться в этом файле!\n"

#define MSGTR_EncryptedVOB "Шифрованный VOB файл! См. DOCS/HTML/ru/dvd.html\n"

#define MSGTR_MOVcomprhdr "MOV: Для поддержки сжатых заголовков необходим zlib!\n"
#define MSGTR_MOVvariableFourCC "MOV: Предупреждение! Обнаружен переменный FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Предупреждение! слишком много треков!"
#define MSGTR_FoundAudioStream "==> Нашёл аудио поток: %d\n"
#define MSGTR_FoundVideoStream "==> Нашёл видео поток: %d\n"
#define MSGTR_DetectedTV "Найден TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Не могу открыть ogg демуксер [demuxer].\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Ищу аудиопоток (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Не могу открыть аудиопоток: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Не могу открыть поток субтитров: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Не могу открыть демуксер [demuxer] аудио: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Не могу открыть демуксер [demuxer] субтитров: %s\n"
#define MSGTR_TVInputNotSeekable "По TV входу нельзя перемещаться! (Возможно перемещение будет для смены каналов ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Информация демуксера [demuxer] %s уже существует!\n"
#define MSGTR_ClipInfo "Информация о клипе:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: обнаружено 30 кадров/сек NTSC содержимое, переключаю частоту кадров.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: обнаружено 24 кадра/сек поступательное [progressive] NTSC содержимое,\nпереключаю частоту кадров.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не могу открыть кодек.\n"
#define MSGTR_CantCloseCodec "Не могу закрыть кодек.\n"

#define MSGTR_MissingDLLcodec "ОШИБКА: Не смог открыть требующийся DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не смог загрузить/инициализировать Win32/ACM AUDIO кодек (потерян DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не могу найти кодек '%s' в libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: ФАТАЛЬНАЯ ОШИБКА: КОНЕЦ ФАЙЛА при поиске последовательности заголовков\n"
#define MSGTR_CannotReadMpegSequHdr "ФАТАЛЬНАЯ ОШИБКА: Не могу считать последовательность заголовков!\n"
#define MSGTR_CannotReadMpegSequHdrEx "ФАТАЛЬНАЯ ОШИБКА: Не мочу считать расширение последовательности заголовков!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Плохая последовательность заголовков!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Плохое расширение последовательности заголовков!\n"

#define MSGTR_ShMemAllocFail "Не могу зарезервировать разделяемую память.\n"
#define MSGTR_CantAllocAudioBuf "Не могу зарезервировать выходной аудио буфер.\n"

#define MSGTR_UnknownAudio "Неизвестный/потерянный аудио формат -> без звука\n"

#define MSGTR_UsingExternalPP "[PP] Использую внешний фильтр постобработки, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Использую постобработку из кодека, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Видео атрибут '%s' не поддерживается выбранными vo и vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Запрошенное семейство видеокодеков [%s] (vfm=%s) не доступно.\nВключите его во время компиляции.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Запрошенное семейство аудиокодеков [%s] (afm=%s) не доступно.\nВключите его во время компиляции.\n"
#define MSGTR_OpeningVideoDecoder "Открываю декодер видео: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Открываю декодер аудио: [%s] %s\n"
#define MSGTR_UninitVideoStr "деинициализация видео: %s\n"
#define MSGTR_UninitAudioStr "деинициализация аудио: %s\n"
#define MSGTR_VDecoderInitFailed "Ошибка инициализации Декодера Видео :(\n"
#define MSGTR_ADecoderInitFailed "Ошибка инициализации Декодера Аудио :(\n"
#define MSGTR_ADecoderPreinitFailed "Ошибка преинициализации Декодера Аудио :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Захватываю %d байт(а/ов) для входного буфера.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Захватываю %d + %d = %d байт(а/ов) для буфера вывода.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Установка поддержки LIRC...\n"
#define MSGTR_LIRCdisabled "Вы не сможете использовать Ваш пульт управления\n"
#define MSGTR_LIRCopenfailed "Неудачное открытие поддержки LIRC!\n"
#define MSGTR_LIRCcfgerr "Неудачная попытка чтения файла конфигурации LIRC '%s'!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Не могу найти видео фильтр '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Не могу открыть видео фильтр '%s'.\n"
#define MSGTR_OpeningVideoFilter "Открываю видео фильтр: "
#define MSGTR_CannotFindColorspace "Не могу найти подходящее цветовое пространство, даже вставив 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Кодек не установил sh->disp_w и sh->disp_h, пытаюсь обойти.\n"
#define MSGTR_VoConfigRequest "VDec: запрос vo config - %d x %d (предпочитаемый csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Не могу найти подходящее цветовое пространство - попытаюсь с -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect - %.2f:1 - премасштабирую для коррекции соотношения сторон фильма.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect не определён - премасштабирование не применяется.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Вам нужно обновить/установить пакет бинарных кодеков.\nЗайдите на http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "ИНФОРМАЦИЯ: Win32/DShow видео кодек успешно инициализирован.\n"
#define MSGTR_DMOInitOK "ИНФОРМАЦИЯ: Win32/DMO видео кодек успешно инициализирован.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Не могу послать событие EWMH fullscreen!\n"

#define MSGTR_InsertingAfVolume "[Микшер] Нет аппаратного микширования, вставляю фильтр громкости.\n"
#define MSGTR_NoVolume "[Микшер] Контроль громкости не доступен.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "О себе"
#define MSGTR_FileSelect "Выбрать файл..."
#define MSGTR_SubtitleSelect "Выбрать субтитры..."
#define MSGTR_OtherSelect "Выбор..."
#define MSGTR_AudioFileSelect "Выбор внешнего аудио канала..."
#define MSGTR_FontSelect "Выбор шрифта..."
#define MSGTR_PlayList "Плейлист"
#define MSGTR_Equalizer "Эквалайзер"
#define MSGTR_ConfigureEqualizer "Настройка каналов"
#define MSGTR_SkinBrowser "Просмотрщик скинов"
#define MSGTR_Network "Сетевые потоки..."
#define MSGTR_Preferences "Настройки"
#define MSGTR_AudioPreferences "Конфигурация аудио драйвера"
#define MSGTR_NoMediaOpened "Носитель не открыт."
#define MSGTR_VCDTrack "VCD дорожка %d"
#define MSGTR_NoChapter "Нет главы"
#define MSGTR_Chapter "Глава %d"
#define MSGTR_NoFileLoaded "Файл не загружен."

// --- buttons ---
#define MSGTR_Ok "Да"
#define MSGTR_Cancel "Отмена"
#define MSGTR_Add "Добавить"
#define MSGTR_Remove "Удалить"
#define MSGTR_Clear "Очистить"
#define MSGTR_Config "Конфигурировать"
#define MSGTR_ConfigDriver "Конфигурировать драйвер"
#define MSGTR_Browse "Просмотреть"

// --- error messages ---
#define MSGTR_NEMDB "Извините, не хватает памяти для буфера прорисовки."
#define MSGTR_NEMFMR "Извините, не хватает памяти для отображения меню."
#define MSGTR_IDFGCVD "Извините, не нашёл совместимый с GUI драйвер видео вывода."
#define MSGTR_NEEDLAVCFAME "Извините, Вы не можете проигрывать не-MPEG файлы на Вашем DXR3/H+ устройстве\nбез перекодирования. Пожалуйста, включите lavc или fame при конфигурации DXR3/H+."
#define MSGTR_UNKNOWNWINDOWTYPE "Найден неизвестный тип окна..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[шкура] ошибка в файле конфигурации шкуры на строке %d: %s" 
#define MSGTR_SKIN_WARNING1 "[шкура] предупреждение: в файле конфигурации скина на строке %d:\nэлемент GUI найден, но до этого не найдено \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[шкура] предупреждение: в файле конфигурации скина на строке %d:\nэлемент GUI найден, но до этого не найдено \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[шкура] предупреждение: в файле конфигурации скина на строке %d:\nэта подсекция не поддерживается этим элементом GUI (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[шкура] файл '%s' не найден.\n"
#define MSGTR_SKIN_BITMAP_16bit  "Глубина битовой матрицы в 16 бит и меньше не поддерживается (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "файл не найден (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "ошибка чтения BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "ошибка чтения TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "ошибка чтения PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE упакованный TGA не поддерживается (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "неизвестный тип файла (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "ошибка преобразования 24-бит в 32-бит (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "неизвестное сообщение: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "не хватает памяти\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Объявлено слишком много шрифтов.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Файл шрифта не найден.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Файл образов шрифта не найден.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "несуществующий идентификатор шрифта (%s)\n"
#define MSGTR_SKIN_UnknownParameter "неизвестный параметр (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Шкура не найдена (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Выбранная шкура '%s' не найдена, пробую 'default'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Ошибка чтения фала конфигурации шкур (%s)\n"
#define MSGTR_SKIN_LABEL "Шкуры:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "О MPlayer"
#define MSGTR_MENU_Open "Открыть..."
#define MSGTR_MENU_PlayFile "Играть файл..."
#define MSGTR_MENU_PlayVCD "Играть VCD..."
#define MSGTR_MENU_PlayDVD "Играть DVD..."
#define MSGTR_MENU_PlayURL "Играть URL..."
#define MSGTR_MENU_LoadSubtitle "Загрузить субтитры..."
#define MSGTR_MENU_DropSubtitle "Убрать субтитры..."
#define MSGTR_MENU_LoadExternAudioFile "Загрузить внешний аудио файл..."
#define MSGTR_MENU_Playing "Воспроизведение"
#define MSGTR_MENU_Play "Играть"
#define MSGTR_MENU_Pause "Пауза"
#define MSGTR_MENU_Stop "Останов"
#define MSGTR_MENU_NextStream "След. поток"
#define MSGTR_MENU_PrevStream "Пред. поток"
#define MSGTR_MENU_Size "Размер"
#define MSGTR_MENU_HalfSize   "Половинный размер"
#define MSGTR_MENU_NormalSize "Нормальный размер"
#define MSGTR_MENU_DoubleSize "Двойной размер"
#define MSGTR_MENU_FullScreen "Полный экран"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Играть диск..."
#define MSGTR_MENU_ShowDVDMenu "Показать DVD меню"
#define MSGTR_MENU_Titles "Ролики"
#define MSGTR_MENU_Title "Ролик %2d"
#define MSGTR_MENU_None "(нет)"
#define MSGTR_MENU_Chapters "Главы"
#define MSGTR_MENU_Chapter "Глава %2d"
#define MSGTR_MENU_AudioLanguages "Аудио языки"
#define MSGTR_MENU_SubtitleLanguages "Язык субтитров"
// TODO: Why is this different from MSGTR_PlayList?
#define MSGTR_MENU_PlayList "Список воспроизведения"
#define MSGTR_MENU_SkinBrowser "Просмотрщик шкур"
#define MSGTR_MENU_Exit "Выход..."
#define MSGTR_MENU_Mute "Отключить звук"
#define MSGTR_MENU_Original "Исходный"
#define MSGTR_MENU_AspectRatio "Соотношение размеров"
#define MSGTR_MENU_AudioTrack "Аудио дорожка"
#define MSGTR_MENU_Track "дорожка %d"
#define MSGTR_MENU_VideoTrack "Видео дорожка"
#define MSGTR_MENU_Subtitles "Субтитры"

// --- equalizer
#define MSGTR_EQU_Audio "Аудио"
#define MSGTR_EQU_Video "Видео"
#define MSGTR_EQU_Contrast "Контраст: "
#define MSGTR_EQU_Brightness "Яркость: "
#define MSGTR_EQU_Hue "Цвет: "
#define MSGTR_EQU_Saturation "Насыщенность: "
#define MSGTR_EQU_Front_Left "Передняя Левая"
#define MSGTR_EQU_Front_Right "Передняя Правая"
#define MSGTR_EQU_Back_Left "Задняя Левая"
#define MSGTR_EQU_Back_Right "Задняя Правая"
#define MSGTR_EQU_Center "Центральная"
#define MSGTR_EQU_Bass "Басы"
#define MSGTR_EQU_All "Все"
#define MSGTR_EQU_Channel1 "Канал 1:"
#define MSGTR_EQU_Channel2 "Канал 2:"
#define MSGTR_EQU_Channel3 "Канал 3:"
#define MSGTR_EQU_Channel4 "Канал 4:"
#define MSGTR_EQU_Channel5 "Канал 5:"
#define MSGTR_EQU_Channel6 "Канал 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Путь"
#define MSGTR_PLAYLIST_Selected "Выбранные файлы"
#define MSGTR_PLAYLIST_Files "Файлы"
#define MSGTR_PLAYLIST_DirectoryTree "Дерево каталогов"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Субтитры и OSD"
#define MSGTR_PREFERENCES_Codecs "Кодеки и демуксер [demuxer]"
#define MSGTR_PREFERENCES_Misc "Разное"

#define MSGTR_PREFERENCES_None "Нет"
#define MSGTR_PREFERENCES_DriverDefault "умолчания драйвера"
#define MSGTR_PREFERENCES_AvailableDrivers "Доступные драйверы:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Не проигрывать звук"
#define MSGTR_PREFERENCES_NormalizeSound "Нормализовать звук"
#define MSGTR_PREFERENCES_EnableEqualizer "Включить эквалайзер"
#define MSGTR_PREFERENCES_ExtraStereo "Включить дополнительное стерео"
#define MSGTR_PREFERENCES_Coefficient "Коэффициент:"
#define MSGTR_PREFERENCES_AudioDelay "Задержка аудио"
#define MSGTR_PREFERENCES_DoubleBuffer "Включить двойную буферизацию"
#define MSGTR_PREFERENCES_DirectRender "Включить прямое отображение"
#define MSGTR_PREFERENCES_FrameDrop "Включить выбрасывание кадров"
#define MSGTR_PREFERENCES_HFrameDrop "Включить СИЛЬНОЕ выбрасывание кадров (опасно)"
#define MSGTR_PREFERENCES_Flip "Отобразить изображение вверх ногами"
#define MSGTR_PREFERENCES_Panscan "Усечение сторон: "
#define MSGTR_PREFERENCES_OSDTimer "Таймер и индикаторы"
#define MSGTR_PREFERENCES_OSDProgress "Только полосы выполнения"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Таймер, проценты и полное время"
#define MSGTR_PREFERENCES_Subtitle "Субтитры:"
#define MSGTR_PREFERENCES_SUB_Delay "Задержка: "
#define MSGTR_PREFERENCES_SUB_FPS "Кадр/сек:"
#define MSGTR_PREFERENCES_SUB_POS "Позиция: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Выключить автозагрузку субтитров"
#define MSGTR_PREFERENCES_SUB_Unicode "Уникодовые субтитры"
#define MSGTR_PREFERENCES_SUB_MPSUB "Конвертировать данные субтитры в MPlayer'овский формат субтитров"
#define MSGTR_PREFERENCES_SUB_SRT "Конвертировать данные субтитры в основанный на времени SubViewer (SRT) формат"
#define MSGTR_PREFERENCES_SUB_Overlap "Изменить перекрывание субтитров"
#define MSGTR_PREFERENCES_Font "Шрифт:"
#define MSGTR_PREFERENCES_FontFactor "Коэффициент шрифта:"
#define MSGTR_PREFERENCES_PostProcess "Включить постобработку"
#define MSGTR_PREFERENCES_AutoQuality "Авто качество: "
#define MSGTR_PREFERENCES_NI "Использовать 'неслоёный' AVI парсер"
#define MSGTR_PREFERENCES_IDX "Если требуется, перестроить индексную таблицу"
#define MSGTR_PREFERENCES_VideoCodecFamily "Семейство видео кодеков:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Семейство аудио кодеков:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "уровень OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Субтитры"
#define MSGTR_PREFERENCES_FRAME_Font "Шрифт"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Постобработка"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Кодек и демуксер [demuxer]"
#define MSGTR_PREFERENCES_FRAME_Cache "Кэш"
#define MSGTR_PREFERENCES_Audio_Device "Устройство:"
#define MSGTR_PREFERENCES_Audio_Mixer "Микшер:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Канал микшера:"
#define MSGTR_PREFERENCES_Message "Пожалуйста, запомните, что Вам нужно перезапустить проигрывание,\nчтобы некоторые изменения вступили в силу!"
#define MSGTR_PREFERENCES_DXR3_VENC "Видео кодировщик:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Использовать LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Использовать FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Уникод"
#define MSGTR_PREFERENCES_FontEncoding2 "Западноевропейские языки (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Западноевропейские языки с Евро (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Славянские/Центрально-европейские языки (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Эсперанто, Галицийский, Мальтийский, Турецкий (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Старая Балтийская кодировка (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Кириллица (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Арабская (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Современная Греческая (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Турецкая (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Балтийская (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Кельтская (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Еврейские кодировки (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Русская (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Украинская, Белорусская (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Упрощённая Китайская кодировка (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Традиционная Китайская кодировка (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Японские кодировки (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Корейская кодировка (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Тайская кодировка (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Кириллица Window$ (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Славянский/Центрально европейский Window$ (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Не масштабировать"
#define MSGTR_PREFERENCES_FontPropWidth "Пропорционально ширине фильма"
#define MSGTR_PREFERENCES_FontPropHeight "Пропорционально высоте фильма"
#define MSGTR_PREFERENCES_FontPropDiagonal "Пропорционально диагонали фильма"
#define MSGTR_PREFERENCES_FontEncoding "Кодировка:"
#define MSGTR_PREFERENCES_FontBlur "Нечёткость:"
#define MSGTR_PREFERENCES_FontOutLine "Контуры:"
#define MSGTR_PREFERENCES_FontTextScale "Масштаб текста:"
#define MSGTR_PREFERENCES_FontOSDScale "Масштаб OSD:"
#define MSGTR_PREFERENCES_Cache "Кэш вкл/выкл"
#define MSGTR_PREFERENCES_CacheSize "Размер кэша: "
#define MSGTR_PREFERENCES_LoadFullscreen "Стартовать в полноэкранном режиме"
#define MSGTR_PREFERENCES_SaveWinPos "Сохранять позицию окна"
#define MSGTR_PREFERENCES_XSCREENSAVER "Останавливать XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Включить полосу воспроизведения"
#define MSGTR_PREFERENCES_AutoSync "Автосинхронизация вкл/выкл"
#define MSGTR_PREFERENCES_AutoSyncValue "Автосинхронизация: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM устройство:"
#define MSGTR_PREFERENCES_DVDDevice "DVD устройство:"
#define MSGTR_PREFERENCES_FPS "FPS фильма:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Показывать окно видео, когда неактивен"
#define MSGTR_ABOUT_UHU "Разработка GUI спонсирована UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Фатальная ошибка!"
#define MSGTR_MSGBOX_LABEL_Error "Ошибка!"
#define MSGTR_MSGBOX_LABEL_Warning "Предупреждение!" 

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] недостаточно памяти для изображения\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] недостаточно памяти для изображения\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] ошибка чтения конфигурационного файла...\n"
#define MSGTR_UnableToSaveOption "[cfg] Не могу сохранить опцию '%s'.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Удаляю субтитры.\n"
#define MSGTR_LoadingSubtitles "[GUI] Загружаю субтитры: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Добавляю видео фильтр: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Удаляю видео фильтр: %s\n"

// mw.c

#define MSGTR_NotAFile "Это не похоже на файл: '%s' !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] Не могу открыть дисплей.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Удалённый дисплей, отключаю XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Извините, ваша система не поддерживает расширение разделяемой памяти X'ов.\n"
#define MSGTR_WS_NoXshape "[ws] Извините, ваша система не поддерживает расширение XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Извините, глубина цвета слишком мала.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Слишком много открытых окон.\n"
#define MSGTR_WS_ShmError "[ws] ошибка расширения разделяемой памяти\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Извините, недостаточно памяти для буфера прорисовки.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS не доступен?\n"
#define MSGTR_WS_DpmsNotEnabled "Не могу включить DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Это не похоже на файл...\n"
#define MSGTR_WS_DDNothing "D&D: Ничего не возвращено!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Извините, выбранное устройство видеовывода не совместимо с этим кодеком.\n"
#define MSGTR_VO_GenericError "Произошла следующая ошибка"
#define MSGTR_VO_UnableToAccess "Не могу получить доступ"
#define MSGTR_VO_ExistsButNoDirectory "уже существует, но не является директорией."
#define MSGTR_VO_DirExistsButNotWritable "Директория вывода уже существует, но не доступна для записи."
#define MSGTR_VO_DirExistsAndIsWritable "Директория вывода уже существует и доступна для записи."
#define MSGTR_VO_CantCreateDirectory "Не могу создать директорию вывода."
#define MSGTR_VO_CantCreateFile "Не могу создать выходной файл."
#define MSGTR_VO_DirectoryCreateSuccess "Директория вывода успешно создана."
#define MSGTR_VO_ParsingSuboptions "Разбираю синтаксис субопций."
#define MSGTR_VO_SuboptionsParsedOK "Синтаксис субопций разобран успешно."
#define MSGTR_VO_ValueOutOfRange "Значение вне допустимого диапазона"
#define MSGTR_VO_NoValueSpecified "Значение не указано."
#define MSGTR_VO_UnknownSuboptions "Неизвестная(ые) субопция(и)"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Прогрессивный JPEG включен."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Прогрессивный JPEG выключен."
#define MSGTR_VO_JPEG_BaselineJPEG "Базовый JPEG включен."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Базовый JPEG выключен."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Режим ASCII включен."
#define MSGTR_VO_PNM_RawMode "'Сырой' режим включен."
#define MSGTR_VO_PNM_PPMType "Будут записаны PPM файлы."
#define MSGTR_VO_PNM_PGMType "Будут записаны PGM файлы."
#define MSGTR_VO_PNM_PGMYUVType "Будут записаны PGMYUV файлы."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Для чередующегося режима необходимо, чтобы высота изображения делилась на 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Не могу выделить память для линейного буфера в чередующемся режиме."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Вход не RGB, не могу разделить хроматичные данные по полям!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Ширина изображения должна делиться на 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Недостаточно памяти для размещения фреймбуфера RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Не могу выделить память или файловый описатель для записи \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Ошибка записи изображения в вывод!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Неизвестное субустройство: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Использую чередующийся режим вывода, верхнее поле первое."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Использую чередующийся режим вывода, нижнее поле первое."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Использую (по умолчанию) поступательный режим кадров."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Драйвер видеовывода pgm был заменён -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Драйвер видеовывода md5 был заменён -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "аудиовывод: модули alsa9 и alsa1x были удалены, используйте -ao alsa взамен.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] инициализация аудио: Не могу открыть устройство микшера %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] инициализация аудио: У микшера аудиокарты отсутствует канал '%s',\nиспользую канал по умолчанию.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] инициализация аудио: Не могу открыть аудиоустройство %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] инициализация аудио: Не могу заблокировать файловый описатель: %s\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] инициализация аудио: Не могу установить аудиоустройство в %d-канальный режим.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] инициализация аудио: драйвер не поддерживает SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Ваш аудиодрайвер НЕ поддерживает select()  ***\n Перекомпилируйте MPlayer с #undef HAVE_AUDIO_SELECT в config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nФатальная ошибка: *** НЕ МОГУ ПОВТОРНО ОТКРЫТЬ / СБРОСИТЬ АУДИОУСТРОЙСТВО (%s) ***\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Соединился с звуковым сервером.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Не могу открыть поток.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Поток открыт.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] размер буфера: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Не могу установить громкость в %d.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Гц не поддерживается, попробуйте \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] Выполнить esd_open_sound не удалось: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] задержка: [сервер: %0.2fs, сеть: %0.2fs] (подстройка %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] не могу открыть поток воспроизведения esd: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB аудио: не могу установить микшер: %s\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Гц не поддерживается, попробуйте преобразовать...\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Файл: %s (%s)\nPCM: Частота воспроизведения: %i Гц Каналы: %s Формат %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Информация: наиболее быстрый дампинг достигается с -vc dummy -vo null\nPCM: Информация: для записи WAVE файлов используйте -ao pcm:waveheader (по умолчанию).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Не могу открыть %s для записи!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Частота воспроизведения: %i Гц Каналы: %s Формат %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] исполльзую %s аудиодрайвер.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Неподдерживаемый аудиоформат: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Не могу инициализировать SDL аудио: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Не могу открыть аудио: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] управление.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] инициализация: Частота воспроизведения: %i Гц Каналы: %s Формат %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] воспроизведение: неверное устройство.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] инициализация: ошибка установки параметров: %s\nНе могу установить требуемую частоту воспроизведения.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] инициализация: AL_RATE не доступен на заданном ресурсе.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] инициализация: ошибка получения параметров: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] инициализация: частота воспроизведения теперь %lf (требуемая частота %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] инициализация: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] инициализация: Не могу отурыть аудиоканал: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] деинициализация: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] сброс: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] пауза аудио: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] возобновление аудио: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: Выполнить SETINFO не удалось.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: запись не удалась."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Не могу открыть аудиоустройство %s, %s -> нет звука.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] инициализация аудио: ваша карта не поддерживает канал %d, %s,\nчастоту воспроизведения %d Гц.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Ваш аудиодрайвер НЕ поддерживает select()  ***\n Перекомпилируйте MPlayer с #undef HAVE_AUDIO_SELECT в config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nФатальная ошибка: *** НЕ МОГУ ПОВТОРНО ОТКРЫТЬ / СБРОСИТЬ АУДИОУСТРОЙСТВО (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] инициализация alsa: запрошенный формат: %d Гц, %d каналов, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] инициализация alsa: не найдена(ы) звуковая(ые) карта(ы).\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] инициализация alsa: запрошен неверный формат (%s) - вывод отключен.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] инициализация alsa: ошибка открытия потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] инициализация alsa: ошибка получения pcm информации: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] инициализация alsa: найдена(о) %d звуковая(ых) карт(а), использую: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] инициализация alsa: ошибка получения информации pcm канала: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] инициализация alsa: ошибка установки параметров: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] инициализация alsa: ошибка установки канала: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] инициализация alsa: ошибка подготовки канала: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] деинициализация alsa: ошибка очистки потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] деинициализация alsa: ошибка сброса буферов потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] деинициализация alsa: ошибка закрытия pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] сброс alsa: ошибка очистки потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] сброс alsa: ошибка сброса буферов потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] сброс alsa: ошибка подготовки канала: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] пауза alsa: ошибка очистки потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] пауза alsa: ошибка сброса буферов потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] возобновление alsa: ошибка подготовки канала: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] воспроизведение alsa: alsa недогружена, сбрасываю поток.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] воспроизведение alsa: ошибка подготовки потока воспроизведения: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] воспроизведение alsa: ошибка записи после сброса: %s - пропускаю.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] воспроизведение alsa: ошибка вывода: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO ПЛАГИН] неверный плагин: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "доступные метки в"
#define MSGTR_AF_LADSPA_WarnNoInputs "ПРЕДУПРЕЖДЕНИЕ! У этого LADSPA плагина отсутствуют аудиовходы.\n  Входящий аудиосигнал будет потерян."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Мульти-канальные (>2) плагины пока что не поддерживаются.\n  Используйте только моно- и стереоплагины."
#define MSGTR_AF_LADSPA_ErrNoOutputs "У этого LADSPA плагина отсутствуют аудиовыходы."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Число аудиовходов и аудиовыходов у LADSPA плагина отличается."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "не могу загрузить"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Не могу найти функцию ladspa_descriptor() в указанном файле библиотеки."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Не могу найти метку в библиотеке плагина."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Не указаны субопции"
#define MSGTR_AF_LADSPA_ErrNoLibFile "Не указан файл библиотеки"
#define MSGTR_AF_LADSPA_ErrNoLabel "Не указана метка фильтра"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Недостаточно настроек указано в командной строке"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Входной параметр #%d меньше нижней границы %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Входной параметр #%d больше верхней границы %0.4f.\n"

