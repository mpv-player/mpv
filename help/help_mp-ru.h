/* Translated by:  Nick Kurshev <nickols_k@mail.ru>,
 *		Dmitry Baryshkov <lumag@qnc.ru>
   Was synced with help_mp-en.h: rev 1.87
 ========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2003 Arpad Gereoffy (см. DOCS!)\n"
"\n";

static char help_text[]=
"Запуск:   mplayer [опции] [URL|path/]filename\n"
"\n"
"Опции:\n"
" -vo <drv[:dev]> выбор драйвера и устройства видео вывода (список см. с '-vo help')\n"
" -ao <drv[:dev]> выбор драйвера и устройства аудио вывода (список см. с '-ao help')\n"
#ifdef HAVE_VCD
" -vcd <номер трека> играть VCD (video cd) трек с устройства вместо файла\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  выбор устройства DVD для авторизации (для шифрованных дисков)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <номер титра> играть DVD титр/трек с устройства вместо файла\n"
" -alang/-slang   выбрать язык аудио/субтитров DVD (двубуквенный код страны)\n"
#endif
" -ss <время>     переместиться на заданную (секунды или ЧЧ:ММ:СС) позицию\n"
" -nosound        без звука\n"
" -fs             опции полноэкранного проигрывания (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   установить разрешение дисплея (использовать с -vm или -zoom)\n"
" -sub <file>     указать файл субтитров (см. также -subfps, -subdelay)\n"
" -playlist <file> указать playlist\n"
" -vid x -aid y   опции для выбора видео (x) и аудио (y) потока для проигрывания\n"
" -fps x -srate y опции для изменения видео (x кадр/сек) и аудио (y Hz) скорости\n"
" -pp <quality>   разрешить постпроцессный фильтр (0-4 для DivX, 0-63 для mpegs)\n"
" -framedrop      разрешить потерю кадров (для медленных машин)\n"
" -wid <ид окна>  использовать существующее окно для видео вывода (полезно для plugger!)\n"
"\n"
"Основные кнопки: (полный список в странице man, также смотри input.conf)\n"
" <-  или ->      перемещение вперед/назад на 10 секунд\n"
" up или down     перемещение вперед/назад на  1 минуту\n"
" pgup or pgdown  перемещение вперед/назад на 10 минут\n"
" < или >         перемещение вперед/назад в playlist'е\n"
" p или ПРОБЕЛ    приостановить фильм (любая клавиша - продолжить)\n"
" q или ESC       остановить воспроизведение и выход\n"
" + или -         регулировать задержку звука по +/- 0.1 секунде\n"
" o               цикличный перебор OSD режимов:  нет / навигация / навигация+таймер\n"
" * или /         прибавить или убавить громкость (нажатие 'm' выбирает master/pcm)\n"
" z или x         регулировать задержку субтитров по +/- 0.1 секунде\n"
"\n"
" * * * ПОДРОБНЕЕ СМ. ДОКУМЕНТАЦИЮ, О ДОПОЛНИТЕЛЬНЫХ ОПЦИЯХ И КЛЮЧАХ ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nВыходим... (%s)\n"
#define MSGTR_Exit_quit "Выход"
#define MSGTR_Exit_eof "Конец файла"
#define MSGTR_Exit_error "Фатальная ошибка"
#define MSGTR_IntBySignal "\nMPlayer прерван сигналом %d в модуле: %s \n"
#define MSGTR_NoHomeDir "Не могу найти HOME каталог\n"
#define MSGTR_GetpathProblem "проблемы в get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Создание файла конфигурации: %s\n"
#define MSGTR_InvalidVOdriver "Недопустимое имя драйвера видео вывода: %s\nСм. '-vo help' чтобы получить список доступных драйверов.\n"
#define MSGTR_InvalidAOdriver "Недопустимое имя драйвера аудио вывода: %s\nСм. '-ao help' чтобы получить список доступных драйверов.\n"
#define MSGTR_CopyCodecsConf "(скопируйте etc/codecs.conf (из исходников MPlayer) в ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Используется встроенный codecs.conf\n"
#define MSGTR_CantLoadFont "Не могу загрузить шрифт: %s\n"
#define MSGTR_CantLoadSub "Не могу загрузить субтитры: %s\n"
#define MSGTR_ErrorDVDkey "Ошибка обработки DVD КЛЮЧА.\n"
#define MSGTR_CmdlineDVDkey "Командная строка DVD требует записанный ключ для дешифрования.\n"
#define MSGTR_DVDauthOk "Авторизация DVD выглядит OK.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: выбранный поток потерян!\n"
#define MSGTR_CantOpenDumpfile "Не могу открыть файл дампа!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Кадр/сек не указаны (или недопустимые) в заголовке! Используйте -fps опцию!\n"
#define MSGTR_TryForceAudioFmtStr "Попытка форсировать семейство аудио кодеков %s ...\n"
#define MSGTR_CantFindAfmtFallback "Не могу найти аудио кодек для форсированного семейства, переход на другие драйвера.\n"
#define MSGTR_CantFindAudioCodec "Не могу найти кодек для аудио формата 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Попытайтесь обновить %s из etc/codecs.conf\n*** Если не помогло - читайте DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Не смог проинициализировать аудио кодек! -> без звука\n"
#define MSGTR_TryForceVideoFmtStr "Попытка форсировать семейство видео кодеков %s ...\n"
#define MSGTR_CantFindVideoCodec "Не могу найти кодек для видео формата 0x%X !\n"
#define MSGTR_VOincompCodec "Sorry, выбранное video_out устройство не совместимо с этим кодеком.\n"
#define MSGTR_CannotInitVO "FATAL: Не могу проинициализировать видео драйвер!\n"
#define MSGTR_CannotInitAO "не могу открыть/проинициализировать аудио устройство -> БЕЗ ЗВУКА\n"
#define MSGTR_StartPlaying "Начало воcпроизведения...\n"
#define MSGTR_SystemTooSlow "\n\n"\
"         *****************************************************************\n"\
"         **** Ваша система слишком МЕДЛЕННА чтобы воспроизводить это! ****\n"\
"         *****************************************************************\n"\
"Возможные причины, проблемы, обходы: \n"\
"- Наиболее частая: плохой/сырой _аудио_ драйвер\n"\
"  - Попытайтесь -ao sdl или используйте ALSA 0.5 или эмуляцию oss на ALSA 0.9.\n"\
"  - Поэкспериментируйте с различными значениями -autosync, начните с 30.\n"\
"- Медленный видео вывод\n"\
"  - Попытайтесь другие -vo driver (список: -vo help) или попытайтесь с -framedrop!\n"\
"- Медленный ЦПУ\n"\
"  - Не пытайтесь воспроизводить большие DVD/DivX на медленных процессорах! попытайтесь -hardframedrop\n"\
"- Битый файл.\n"\
"  - Попытайтесь различные комбинации: -nobps  -ni  -mc 0  -forceidx\n"\
"- Медленный носитель (смонтированные NFS/SMB, DVD, VCD и т. п.)\n"\
"  - Используйте -cache 8192.\n"\
"- Используете ли Вы -cache для проигрывания не-'слоеных'[non-interleaved] AVI файлов?\n"\
"  - Используйте -nocache.\n"\
"Читайте DOCS/video.html и DOCS/sound.html для советов по подстройке/ускорению.\n"\
"Если ничего не помогло, тогда читайте DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer был скомпилирован БЕЗ поддержки GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI требует X11!\n"
#define MSGTR_Playing "Проигрывание %s\n"
#define MSGTR_NoSound "Аудио: без звука!!!\n"
#define MSGTR_FPSforced "Кадры/сек форсированы в %5.3f (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Скомпилировано с Определением типа процессора во время выполнения - ПРЕДУПРЕЖДЕНИЕ - это не оптимально!\nДля получения максимальной производительности, перекомпилируйте MPlayer c --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Скомпилировано для x86 CPU со следующими расширениями:"
#define MSGTR_AvailableVideoOutputPlugins "Доступные плагины вывода видео:\n"
#define MSGTR_AvailableVideoOutputDrivers "Доступные драйвера вывода видео:\n"
#define MSGTR_AvailableAudioOutputDrivers "Доступные драйвера вывода звука:\n"
#define MSGTR_AvailableAudioCodecs "Доступные аудио кодеки:\n"
#define MSGTR_AvailableVideoCodecs "Доступные видео кодеки:\n"
#define MSGTR_AvailableAudioFm "\nДоступные (вкомпилированные) семейства/драйверы аудио кодеков:\n"
#define MSGTR_AvailableVideoFm "\nДоступные (вкомпилированные) семейства/драйверы видео кодеков:\n"
#define MSGTR_UsingRTCTiming "Используется аппаратная Linux RTC синхронизация (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Видео: Не могу прочитать свойства.\n"
#define MSGTR_NoStreamFound "Поток не найден.\n"
#define MSGTR_ErrorInitializingVODevice "Ошибка при открытии/инициализации выбранного устройства видео вывода (-vo).\n"
#define MSGTR_ForcedVideoCodec "Форсирован видео кодек: %s\n"
#define MSGTR_ForcedAudioCodec "Форсирован аудио кодек: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Описание: %s\nAO: Автор: %s\n"
#define MSGTR_AOComment "AO: Комментарий: %s\n"
#define MSGTR_Video_NoVideo "Видео: нет видео\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Не могу инициализировать видео фильтры (-vop) или видео вывод (-vo).\n"
#define MSGTR_Paused "\n================= ПРИОСТАНОВЛЕНО =================\r"
#define MSGTR_PlaylistLoadUnable "\nНе могу загрузить плейлист %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer сломался из-за 'Неправильной инструкции'.\n"\
"  Это может быть ошибкой нашего нового кода определения типа CPU во время выполнения...\n"\
"  Пожалуйста, читайте DOCS/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer сломался из-за 'Неправильной инструкции'.\n"\
"  Обычно, это происходит когда вы его запускаете на CPU, отличном от того, для которого\n"\
"  он был скомпилирован/оптимизирован.\n  Проверьте это!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer сломался из-за плохого использования CPU/FPU/RAM.\n"\
"  Перекомпилируйте MPlayer с --enable-debug и сделайте 'gdb' backtrace и\n"\
"  дизассемблирование. Для подробностей, см. DOCS/bugreports.html#crash.b.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer сломался. Это не должно происходить.\n"\
"  Это может быть ошибкой в коде MPlayer'а _или_ в Вашем драйвере _или_ Вашей версии \n"\
"  gcc. Если Вы думаете, что в этом виноват MPlayer, пожалуйста, простите DOCS/bugreports.html\n"\
"  и следуйте инструкциям оттуда. Мы сможем и не будем помогать, если Вы не предоставите\n"\
"  эту информацию, сообщая о возможной ошибке.\n"


// mencoder.c:

#define MSGTR_MEncoderCopyright "(C) 2000-2003 Arpad Gereoffy (см. DOCS)\n"
#define MSGTR_UsingPass3ControllFile "Использую контролирующий файл для 3 прохода: %s\n"
#define MSGTR_MissingFilename "\nПропущено имя файла.\n\n"
#define MSGTR_CannotOpenFile_Device "Не могу открыть файл/устройство.\n"
#define MSGTR_ErrorDVDAuth "Ошибка при DVD авторизации.\n"
#define MSGTR_CannotOpenDemuxer "Не могу открыть демуксер[demuxer].\n"
#define MSGTR_NoAudioEncoderSelected "\nКодировщик аудио (-oac) не выбран. Выберете один или используйте -nosound. Используйте -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\nКодировщик видео (-ovc) не выбран. Выберете один, используйте -ovc help!\n"
#define MSGTR_InitializingAudioCodec "Инициализация аудио кодека...\n"
#define MSGTR_CannotOpenOutputFile "Не могу открыть файл '%s'для вывода.\n"
#define MSGTR_EncoderOpenFailed "Не могу открыть кодировщик.\n"
#define MSGTR_ForcingOutputFourcc "Выходной fourcc форсирован в %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Пишу AVI заголовок...\n"
#define MSGTR_DuplicateFrames "\n%d повторяющийся(хся) кадр(а/ов)!\n"
#define MSGTR_SkipFrame "\nПропускаю кадр!\n"
#define MSGTR_ErrorWritingFile "%s: Ошибка при записи файла.\n"
#define MSGTR_WritingAVIIndex "\nПишу AVI индекс...\n"
#define MSGTR_FixupAVIHeader "Поправляю AVI заголовок...\n"
#define MSGTR_RecommendedVideoBitrate "Рекомендуемый битпоток для %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nПоток видео: %8.3f kbit/s  (%d bps)  размер: %d байт(а/ов)  %5.3f сек.  %d кадр(а/ов)\n"
#define MSGTR_AudioStreamResult "\nПоток аудио: %8.3f kbit/s  (%d bps)  размер: %d байт(а/ов)  %5.3f сек.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     метод переменного битпотока\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(по умолчанию)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           усредненный битпоток\n"\
"\n"\
" cbr           постоянный битпоток\n"\
"               Также форсирует CBR режим кодирования в некоторых предустановленных ABR режимах\n"\
"\n"\
" br=<0-1024>   укажите битпоток в kBit (только CBR и ABR)\n"\
"\n"\
" q=<0-9>       качество (0-высшее, 9-наименьшее) (только для VBR)\n"\
"\n"\
" aq=<0-9>      качество алгоритма (0-лучшее/самый медленный, 9-худшее/быстрейший)\n"\
"\n"\
" ratio=<1-100> коэффициент сжатия\n"\
"\n"\
" vol=<0-10>    установите усиление входящего аудио\n"\
"\n"\
" mode=<0-3>    (по-умолчанию: автоматический)\n"\
"                0: стерео\n"\
"                1: объединенное стерео[joint-stereo]\n"\
"                2: двухканальный\n"\
"                3: моно\n"\
"\n"\
" padding=<0-2>\n"\
"                0: нет\n"\
"                1: все\n"\
"                2: регулируемое\n"\
"\n"\
" fast          переключиться на быстрое кодирование в некоторых предустановленных VBR\n"\
"               режимах, значительно худшее качество и высокие битпотоки.\n"\
"\n"\
" preset=<value> представляют наибольшие возможные установки качества.\n"\
"                 medium: VBR  кодирование,  хорошее качество\n"\
"                 (амплитуда битпотока - 150-180 kbps)\n"\
"                 standard:  VBR кодирование, высокое качество\n"\
"                 (амплитуда битпотока - 170-210 kbps)\n"\
"                 extreme: VBR кодирование, очень высокое качество\n"\
"                 (амплитуда битпотока - 200-240 kbps)\n"\
"                 insane:  CBR кодирование, лучшее предустановленное качество\n"\
"                 (битпоток 320 kbps)\n"\
"                 <8-320>: ABR кодирование с заданным в kbit'ах средним битпотоком.\n\n"



// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM '%s' не найден!\n"
#define MSGTR_ErrTrackSelect "Ошибка выбора трека VCD!"
#define MSGTR_ReadSTDIN "Чтение из stdin...\n"
#define MSGTR_UnableOpenURL "Не могу открыть URL: %s\n"
#define MSGTR_ConnToServer "Соединение с сервером: %s\n"
#define MSGTR_FileNotFound "Файл не найден: '%s'\n"

#define MSGTR_SMBInitError "Не могу проинициализировать библиотеку libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Не могу открыть по сети: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer не был скомпилирован с поддержкой чтения SMB\n"

#define MSGTR_CantOpenDVD "Не смог открыть DVD: %s\n"
#define MSGTR_DVDwait "Чтение структуры диска, подождите пожалуйста...\n"
#define MSGTR_DVDnumTitles "Есть %d титров на этом DVD.\n"
#define MSGTR_DVDinvalidTitle "Недопустимый номер DVD титра: %d\n"
#define MSGTR_DVDnumChapters "Есть %d глав в этом DVD титре.\n"
#define MSGTR_DVDinvalidChapter "Недопустимый номер DVD главы: %d\n"
#define MSGTR_DVDnumAngles "Есть %d углов в этом DVD титре.\n"
#define MSGTR_DVDinvalidAngle "Недопустимый номер DVD угла: %d\n"
#define MSGTR_DVDnoIFO "Не могу открыть IFO файл для DVD титра %d.\n"
#define MSGTR_DVDnoVOBs "Не могу открыть титр VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD успешно открыт!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Предупреждение! Заголовок аудио потока %d переопределен!\n"
#define MSGTR_VideoStreamRedefined "Предупреждение! Заголовок видео потока %d переопределен!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Слишком много (%d в %d байтах) аудио пакетов в буфере!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Слишком много (%d в %d байтах) видео пакетов в буфере!\n"
#define MSGTR_MaybeNI "Возможно Вы проигрываете нечередованный поток/файл или неудачный кодек?\n" \
                     "Для AVI файлов попробуйте форсировать нечередованный режим опцией -ni.\n"
#define MSGTR_SwitchToNi "\nОбнаружен плохо чередованный AVI файл -переключаюсь в -ni режим...\n"
#define MSGTR_Detected_XXX_FileFormat "Обнаружен %s формат файла!\n"
#define MSGTR_DetectedAudiofile "Обнаружен аудио файл.\n"
#define MSGTR_NotSystemStream "Не MPEG System Stream формат... (возможно Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Недопустимый MPEG-ES поток??? свяжитесь с автором, это может быть багом :(\n"
#define MSGTR_FormatNotRecognized "========= Sorry, формат этого файла не распознан/не поддерживается ===========\n"\
				  "===== Если это AVI, ASF или MPEG поток, пожалуйста свяжитесь с автором! ======\n"
#define MSGTR_MissingVideoStream "Видео поток не найден!\n"
#define MSGTR_MissingAudioStream "Аудио поток не найден...  ->без звука\n"
#define MSGTR_MissingVideoStreamBug "Видео поток потерян!? свяжитесь с автором, это может быть багом :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: файл не содержит выбранный аудио или видео поток\n"

#define MSGTR_NI_Forced "Форсирован"
#define MSGTR_NI_Detected "Обнаружен"
#define MSGTR_NI_Message "%s НЕЧЕРЕДОВАННЫЙ формат AVI файла!\n"

#define MSGTR_UsingNINI "Использование НЕЧЕРЕДОВАННОГО испорченного формата AVI файла!\n"
#define MSGTR_CouldntDetFNo "Не смог определить число кадров (для абсолютного перемещения)\n"
#define MSGTR_CantSeekRawAVI "Не могу переместиться в сыром потоке AVI! (требуется индекс, попробуйте с ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не могу перемещаться в этом файле!\n"

#define MSGTR_EncryptedVOB "Шифрованный VOB файл (не компилировали с поддержкой libcss)! См. DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Шифрованный поток но авторизация Вами не была затребована!!\n"

#define MSGTR_MOVcomprhdr "MOV: Сжатые заголовки (пока) не поддерживаются!\n"
#define MSGTR_MOVvariableFourCC "MOV: Предупреждение! Обнаружен переменный FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Предупреждение! слишком много треков!"
#define MSGTR_FoundAudioStream "==> Нашел аудио поток: %d\n"
#define MSGTR_FoundVideoStream "==> Нашел видео поток: %d\n"
#define MSGTR_DetectedTV "Найден TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Не могу открыть ogg демуксер[demuxer].\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Ищу аудио поток (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Не могу открыть аудио поток: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Не могу открыть поток субтитров: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Не могу открыть демуксер[demuxer] аудио: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Не могу открыть демуксер[demuxer] субтитров: %s\n"
#define MSGTR_TVInputNotSeekable "По TV входу нельзя перемещаться! (Возможно перемещение будет для смены каналов ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Информация демуксера[demuxer] %s уже существует!\n"
#define MSGTR_ClipInfo "Информация о клипе:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: обнаружена продолжительная[Progressive] последовательность, покидаю 3:2 TELECINE режим\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 3:2 TELECINE обнаружено, включаю обратный telecine fx. FPS изменено в %5.3f!  \n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не смог открыть кодек\n"
#define MSGTR_CantCloseCodec "Не смог закрыть кодек\n"

#define MSGTR_MissingDLLcodec "ОШИБКА: Не смог открыть требующийся DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не смог загрузить/проинициализировать Win32/ACM AUDIO кодек (потерян DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не могу найти кодек '%s' в libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: КОНЕЦ ФАЙЛА при поиске последовательности заголовков\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не могу читать последовательность заголовков!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читать расширение последовательности заголовков!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Плохая последовательность заголовков!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Плохое расширение последовательности заголовков!\n"

#define MSGTR_ShMemAllocFail "Не могу захватить общую память\n"
#define MSGTR_CantAllocAudioBuf "Не могу захватить выходной буфер аудио\n"

#define MSGTR_UnknownAudio "Неизвестный/потерянный аудио формат, отказ от звука\n"

#define MSGTR_UsingExternalPP "[PP] Использую внешний постпроцессный фильтр, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Использую построцессирование кодека, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Видео атрибут '%s' не поддерживается выбранными vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Запрошенное семейство видео кодеков [%s] (vfm=%s) не доступно (включите во время компиляции)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Запрошенное семейство аудио кодеков [%s] (afm=%s) не доступно (включите во время компиляции)\n"
#define MSGTR_OpeningVideoDecoder "Открываю декодер видео: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Открываю декодер аудио: [%s] %s\n"
#define MSGTR_UninitVideoStr "деинициализация видео: %s\n"
#define MSGTR_UninitAudioStr "деинициализация аудио: %s\n"
#define MSGTR_VDecoderInitFailed "Ошибка инициализации ВидеоДекодера :(\n"
#define MSGTR_ADecoderInitFailed "Ошибка инициализации АудиоДекодера :(\n"
#define MSGTR_ADecoderPreinitFailed "Ошибка преинициализации АудиоДекодера :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Захватываю %d байт(а/ов) для входного буфера\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Захватываю %d + %d = %d байт(а/ов) для буфера вывода\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Установка поддержки lirc...\n"
#define MSGTR_LIRCdisabled "Вы не сможете использовать Ваше удаленное управление\n"
#define MSGTR_LIRCopenfailed "Неудачное открытие поддержки lirc!\n"
#define MSGTR_LIRCcfgerr "Неудачное чтение файла конфигурации LIRC %s !\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Не могу найти видео фильтр '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Не могу открыть видео фильтр '%s'\n"
#define MSGTR_OpeningVideoFilter "Открываю видео фильтр: "
#define MSGTR_CannotFindColorspace "Не могу найти общее цветовое пространство, даже вставив 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Кодек не установил sh->disp_w и sh->disp_h, попытаюсь обойти.\n"
#define MSGTR_VoConfigRequest "VDec: vo config запросил - %d x %d (предпочитаемый csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Не могу найти подходящее цветовое пространство - попытаюсь с -vop scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect - %.2f:1 - премасштабирую для коррекции соотношения сторон фильма.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect не определен - премасштабирование не применяется.\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "О себе"
#define MSGTR_FileSelect "Выбрать файл ..."
#define MSGTR_SubtitleSelect "Выбрать субтитры ..."
#define MSGTR_OtherSelect "Выбор ..."
#define MSGTR_AudioFileSelect "Выбор внешнего аудио канала ..."
#define MSGTR_FontSelect "Выбор шрифта ..."
#define MSGTR_PlayList "Плейлист"
#define MSGTR_Equalizer "Эквалайзер"
#define MSGTR_SkinBrowser "Просмотрщик скинов"
#define MSGTR_Network "Сетевые потоки ..."
#define MSGTR_Preferences "Настройки"
#define MSGTR_OSSPreferences "конфигурация OSS драйвера"
#define MSGTR_SDLPreferences "конфигурация SDL драйвера"
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
#define MSGTR_NEMDB "Sorry, не хватает памяти для отрисовки буфера."
#define MSGTR_NEMFMR "Sorry, не хватает памяти для отображения меню."
#define MSGTR_IDFGCVD "Sorry, не нашел совместимый с GUI драйвер видео вывода."
#define MSGTR_NEEDLAVCFAME "Sorry, Вы не можете проигрывать не-MPEG файлы на Вашем DXR3/H+устройстве без перекодирования.\nПожалуйста, включите lavc или fame при конфигурации DXR3/H+."


// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] ошибка в файле конфигурации скина на строке %d: %s" 
#define MSGTR_SKIN_WARNING1 "[skin] предупреждение: в файле конфигурации скина на строке %d: widget найден но до этого не найдена \"section\" ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] предупреждение: в файле конфигурации скина на строке %d: widget найден но до этого не найдена \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] предупреждение: в файле конфигурации скина на строке %d: эта подсекция не поддерживается этим виджетом[widget] (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Глубина bitmap в 16 бит и меньше не поддерживается ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "файл не найден ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "ошибка чтения bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "ошибка чтения tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "ошибка чтения png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE упакованный tga не поддерживается ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "неизвестный тип файла ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "ошибка преобразования 24-бит в 32-бит ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "неизвестное сообщение: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "не хватает памяти\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "объявлено слишком много шрифтов\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "файл шрифта не найден\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "файл образов шрифта не найден\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "несуществующий идентификатор шрифта ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "неизвестный параметр ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] не хватает памяти.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin не найден ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Ошибка чтения фала конфигурации skin ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "О MPlayer"
#define MSGTR_MENU_Open "Открыть ..."
#define MSGTR_MENU_PlayFile "Играть файл ..."
#define MSGTR_MENU_PlayVCD "Играть VCD ..."
#define MSGTR_MENU_PlayDVD "Играть DVD ..."
#define MSGTR_MENU_PlayURL "Играть URL ..."
#define MSGTR_MENU_LoadSubtitle "Загрузить субтитры ..."
#define MSGTR_MENU_DropSubtitle "Убрать субтитры ..."
#define MSGTR_MENU_LoadExternAudioFile "Загрузить внешний аудио файл ..."
#define MSGTR_MENU_Playing "Воспроизведение"
#define MSGTR_MENU_Play "Играть"
#define MSGTR_MENU_Pause "Пауза"
#define MSGTR_MENU_Stop "Останов"
#define MSGTR_MENU_NextStream "След. поток"
#define MSGTR_MENU_PrevStream "Пред. поток"
#define MSGTR_MENU_Size "Размер"
#define MSGTR_MENU_NormalSize "Нормальный размер"
#define MSGTR_MENU_DoubleSize "Двойной размер"
#define MSGTR_MENU_FullScreen "Полный экран"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Играть диск ..."
#define MSGTR_MENU_ShowDVDMenu "Показать DVD меню"
#define MSGTR_MENU_Titles "Титры"
#define MSGTR_MENU_Title "Титр %2d"
#define MSGTR_MENU_None "(нет)"
#define MSGTR_MENU_Chapters "Главы"
#define MSGTR_MENU_Chapter "Глава %2d"
#define MSGTR_MENU_AudioLanguages "Авто язык"
#define MSGTR_MENU_SubtitleLanguages "Язык субтитров"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Просмотрщик skin'ов"
#define MSGTR_MENU_Preferences "Настройки"
#define MSGTR_MENU_Exit "Выход ..."
#define MSGTR_MENU_Mute "Отключить звук"
#define MSGTR_MENU_Original "Исходный"
#define MSGTR_MENU_AspectRatio "Соотношение размеров"
#define MSGTR_MENU_AudioTrack "Аудио дорожка"
#define MSGTR_MENU_Track "дорожка %d"
#define MSGTR_MENU_VideoTrack "Видео дорожка"


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
#define MSGTR_PREFERENCES_Audio "Аудио"
#define MSGTR_PREFERENCES_Video "Видео"
#define MSGTR_PREFERENCES_SubtitleOSD "Субтитры & OSD"
#define MSGTR_PREFERENCES_Codecs "Кодеки & демуксер[demuxer]"
#define MSGTR_PREFERENCES_Misc "Разное"

#define MSGTR_PREFERENCES_None "Нет"
#define MSGTR_PREFERENCES_AvailableDrivers "Доступные драйверы:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Не проигрывать звук"
#define MSGTR_PREFERENCES_NormalizeSound "Нормализовать звук"
#define MSGTR_PREFERENCES_EnEqualizer "Включить эквалайзер"
#define MSGTR_PREFERENCES_ExtraStereo "Включить дополнительное стерео"
#define MSGTR_PREFERENCES_Coefficient "Коэффициент:"
#define MSGTR_PREFERENCES_AudioDelay "Задержка аудио"
#define MSGTR_PREFERENCES_DoubleBuffer "Включить двойную буферизацию"
#define MSGTR_PREFERENCES_DirectRender "Включить прямое отображение"
#define MSGTR_PREFERENCES_FrameDrop "Включить выбрасывание кадров"
#define MSGTR_PREFERENCES_HFrameDrop "Включить HARD выбрасывание кадров (опасно)"
#define MSGTR_PREFERENCES_Flip "Отобразить изображение вверх ногами"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Таймер и индикаторы"
#define MSGTR_PREFERENCES_OSDProgress "Только progressbar'ы"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Таймер, проценты и полное время"
#define MSGTR_PREFERENCES_Subtitle "Субтитры:"
#define MSGTR_PREFERENCES_SUB_Delay "Задержка: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Позиция: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Выключить автозагрузку субтитров"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode'овые субтитры"
#define MSGTR_PREFERENCES_SUB_MPSUB "Конвертировать данные субтитры в MPlayer'овский формат субтитров"
#define MSGTR_PREFERENCES_SUB_SRT "Конвертировать данные субтитры в основанный на времени SubViewer (SRT) формат"
#define MSGTR_PREFERENCES_SUB_Overlap "Изменить перекрывание субтитров"
#define MSGTR_PREFERENCES_Font "Шрифт:"
#define MSGTR_PREFERENCES_FontFactor "Коэффициент шрифта:"
#define MSGTR_PREFERENCES_PostProcess "Включить постпроцессинг"
#define MSGTR_PREFERENCES_AutoQuality "Авто качество: "
#define MSGTR_PREFERENCES_NI "Использовать нечередованный AVI парсер"
#define MSGTR_PREFERENCES_IDX "Если требуется, создавать индексную таблицу"
#define MSGTR_PREFERENCES_VideoCodecFamily "Семейство видео кодеков:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Семейство аудио кодеков:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "уровень OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Субтитры"
#define MSGTR_PREFERENCES_FRAME_Font "Шрифт"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Постпроцессинг"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Кодек & демуксер[demuxer]"
#define MSGTR_PREFERENCES_FRAME_Cache "Кэш"
#define MSGTR_PREFERENCES_FRAME_Misc "Разное"
#define MSGTR_PREFERENCES_OSS_Device "Устройство:"
#define MSGTR_PREFERENCES_OSS_Mixer "Микшер:"
#define MSGTR_PREFERENCES_SDL_Driver "Драйвер:"
#define MSGTR_PREFERENCES_Message "Пожалуйста, запомните, что Вам нужно перезапустить проигрывание, чтобы некоторые изменения вступили в силу!"
#define MSGTR_PREFERENCES_DXR3_VENC "Видео кодировщик:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Использовать LAVC (ffmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Использовать FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Западноевропейские языки (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Западноевропейские языки с Евро (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Славянские/Центральноевропейские языки (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Эсперанто, Galician, Мальтийский, Турецкий (ISO-8859-3)"
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
#define MSGTR_PREFERENCES_FontEncoding16 "Упрощенная Китайская кодировка (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Традиционная Китайская кодировка (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Японские кодировки (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Корейская кодировка (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Тайская кодировка (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Кириллица Windows (CP1251)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Не масштабировать"
#define MSGTR_PREFERENCES_FontPropWidth "Пропорционально ширине фильма"
#define MSGTR_PREFERENCES_FontPropHeight "Пропорционально высоте фильма"
#define MSGTR_PREFERENCES_FontPropDiagonal "Пропорционально диагонали фильма"
#define MSGTR_PREFERENCES_FontEncoding "Кодировка:"
#define MSGTR_PREFERENCES_FontBlur "Нечеткость:"
#define MSGTR_PREFERENCES_FontOutLine "Контуры:"
#define MSGTR_PREFERENCES_FontTextScale "Масштаб текста:"
#define MSGTR_PREFERENCES_FontOSDScale "Масштаб OSD:"
#define MSGTR_PREFERENCES_Cache "Кэш вкл/выкл"
#define MSGTR_PREFERENCES_CacheSize "Размер кэша: "
#define MSGTR_PREFERENCES_LoadFullscreen "Стартовать на полный экран"
#define MSGTR_PREFERENCES_XSCREENSAVER "Останавливать XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Включить playbar"
#define MSGTR_PREFERENCES_AutoSync "AutoSync вкл/выкл"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM устройство:"
#define MSGTR_PREFERENCES_DVDDevice "DVD устройство:"
#define MSGTR_PREFERENCES_FPS "FPS фильма:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Показывать Окно Видео, когда неактивен"

#define MSGTR_ABOUT_UHU "Разработка GUI спонсирована UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   Основная команда MPlayer'а:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Дополнительные кодеры:\n"
#define MSGTR_ABOUT_MainTesters "   Главные тестеры:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "фатальная ошибка ..."
#define MSGTR_MSGBOX_LABEL_Error "ошибка ..."
#define MSGTR_MSGBOX_LABEL_Warning "предупреждение ..." 

#endif
