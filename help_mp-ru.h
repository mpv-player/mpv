/* Translated by:  Nick Kurshev <nickols_k@mail.ru>
   Was synced with help_mp-en.h: rev 1.20
 ========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (см. DOCS!)\n"
"\n";

static char help_text[]=
"Запуск:   mplayer [опции] [path/]filename\n"
"\n"
"Опции:\n"
" -vo <drv[:dev]> выбор драйвера и устройства видео вывода (список см. с '-vo help')\n"
" -ao <drv[:dev]> выбор драйвера и устройства аудио вывода (список см. с '-ao help')\n"
" -vcd <номер трека> играть VCD (video cd) трек с устройства виесто файла\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  выбор устройства DVD для авторизации (для шифрованных дисков)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <номер титра> играть DVD титр/трек с устройства вместо файла\n"
#endif
" -ss <время>     переместиться на заданную (секунды или ЧЧ:ММ:СС) позицию\n"
" -nosound        без звука\n"
#ifdef USE_FAKE_MONO
" -stereo <режим> выбор MPEG1 стерео вывода (0:стерео 1:левый 2:правый)\n"
#endif
" -channels <n>   номер выходных каналов звука\n"
" -fs -vm -zoom   опции полноэкранного проигрывания (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   масштабировать картинку в <x> * <y> разрешение [если -vo драйвер поддерживает!]\n"
" -sub <file>     указать файл субтитров (см. также -subfps, -subdelay)\n"
" -playlist <file> указать playlist\n"
" -vid x -aid y   опции для выбора видео (x) и аудио (y) потока для проишрывания\n"
" -fps x -srate y опции для изменения видео (x кадр/сек) и аудио (y Hz) скорости\n"
" -pp <quality>   разрешить постпроцессный фильтр (0-4 для DivX, 0-63 для mpegs)\n"
" -nobps          использовать альтернативный метод синхронизации A-V для AVI файлов (может помочь!)\n"
" -framedrop      разрешить потерю кадров (для медленных машин)\n"
" -wid <ид окна>  использовать существующее окно для видео вывода (полезно для plugger!)\n"
"\n"
"Ключи:\n"
" <-  или ->      перемещение вперед/назад на 10 секунд\n"
" up или down     перемещение вперед/назад на  1 минуту\n"
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
#define MSGTR_CantLoadFont "Не могу загрузить шрифт: %s\n"
#define MSGTR_CantLoadSub "Не могу загрузить субтитры: %s\n"
#define MSGTR_ErrorDVDkey "Ошибка обработки DVD КЛЮЧА.\n"
#define MSGTR_CmdlineDVDkey "Коммандная строка DVD требует записанный ключ для дешифрования.\n"
#define MSGTR_DVDauthOk "Авторизация DVD выглядит OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: выбранный поток потерян!\n"
#define MSGTR_CantOpenDumpfile "Не могу открыть файл дампа!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Кадр/сек не указаны (или недопустимые) в заголовке! Используйте -fps опцию!\n"
#define MSGTR_TryForceAudioFmt "Попытка форсировать семейство аудио кодеков %d ...\n"
#define MSGTR_CantFindAfmtFallback "Не могу найти аудио кодек для форсированного семейства, переход на другие драйвера.\n"
#define MSGTR_CantFindAudioCodec "Не могу найти кодек для аудио формата 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Попытайтесь обновить %s из etc/codecs.conf\n*** Если не помогло - читайте DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Не смог проинициализировать аудио кодек! -> без звука\n"
#define MSGTR_TryForceVideoFmt "Попытка форсировать семество видео кодеков %d ...\n"
#define MSGTR_CantFindVideoCodec "Не могу найти кодек для видео формата 0x%X !\n"
#define MSGTR_VOincompCodec "Sorry, выбранное video_out устройство не совместимо с этим кодеком.\n"
#define MSGTR_CannotInitVO "FATAL: Не могу проинициализировать видео драйвер!\n"
#define MSGTR_CannotInitAO "не могу открыть/проинициализировать аудио устройство -> БЕЗ ЗВУКА\n"
#define MSGTR_StartPlaying "Начало воcпроизведения...\n"
#define MSGTR_SystemTooSlow "\n\n"\
"         *****************************************************************\n"\
"         **** Ваша система слишком МЕДЛЕННА чтобы воспроизводить это! ****\n"\
"         *****************************************************************\n"\
"!!! Возможные причины, проблемы, обходы: \n"\
"- Наиболее общие: плохой/сырой _аудио_ драйвер. обход: попытпйтесь -ao sdl или\n"\
"  используйте ALSA 0.5 или эмуляцию oss на ALSA 0.9. Читайте DOCS/sound.html!\n"\
"- Медленный видео вывод. Попытайтесь другие -vo driver (список: -vo help) или\n"\
"  попытайтесь с -framedrop ! Читайте DOCS/video.html.\n"\
"- Медленный ЦПУ. Не пытайтесь воспроизводить большие dvd/divx на медленных\n"\
"  процессорах! попытайтесь -hardframedrop\n"\
"- Битый файл. Попытайтесь различные комбинации: -nobps  -ni  -mc 0  -forceidx\n"\
"Если ничего не помогло, тогда читайте DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer был скомрпилен БЕЗ поддержки GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI требует X11!\n"
#define MSGTR_Playing "Проигрывание %s\n"
#define MSGTR_NoSound "Аудио: без звука!!!\n"
#define MSGTR_FPSforced "Кадры/сек форсированы в %5.3f (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM '%s' не найден!\n"
#define MSGTR_ErrTrackSelect "Ошибка выбора трека VCD!"
#define MSGTR_ReadSTDIN "Чтение из stdin...\n"
#define MSGTR_UnableOpenURL "Не моге открыть URL: %s\n"
#define MSGTR_ConnToServer "Соединение с сервером: %s\n"
#define MSGTR_FileNotFound "Файл не найден: '%s'\n"

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
#define MSGTR_MaybeNI "(возможно Вы проигрываете нечередованный поток/файл или неудачный кодек)\n"
#define MSGTR_DetectedFILMfile "Обнаружен FILM формат файла!\n"
#define MSGTR_DetectedFLIfile "Обнаружен FLI формат файла!\n"
#define MSGTR_DetectedROQfile "Обнаружен RoQ формат файла!\n"
#define MSGTR_DetectedREALfile "Обнаружен REAL формат файла!\n"
#define MSGTR_DetectedAVIfile "Обнаружен AVI формат файла!\n"
#define MSGTR_DetectedASFfile "Обнаружен ASF формат файла!\n"
#define MSGTR_DetectedMPEGPESfile "Обнаружен MPEG-PES формат файла!\n"
#define MSGTR_DetectedMPEGPSfile "Обнаружен MPEG-PS формат файла!\n"
#define MSGTR_DetectedMPEGESfile "Обнаружен MPEG-ES формат файла!\n"
#define MSGTR_DetectedQTMOVfile "Обнаружен QuickTime/MOV формат файла!\n"
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
#define MSGTR_CantSeekRawAVI "Не могу переместиться в сыром потоке .AVI! (требуется индекс, попробуйте с ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не могу перемещаться в этом файле!\n"

#define MSGTR_EncryptedVOB "Шифрованный VOB файл (не компилили с поддержкой libcss)! См. DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Шифрованный поток но авторизация Вами не была затребована!!\n"

#define MSGTR_MOVcomprhdr "MOV: Сжатые заголовки (пока) не поддерживаются!\n"
#define MSGTR_MOVvariableFourCC "MOV: Предупреждение! Обнаружен переменный FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Предупреждение! слишком много треков!"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не смог открыть кодек\n"
#define MSGTR_CantCloseCodec "Не смог закрыть кодек\n"

#define MSGTR_MissingDLLcodec "ОШИБКА: Не смог открыть требующийся DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не смог загрузить/проинициализировать Win32/ACM AUDIO кодек (потерян DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не могу найти кодек '%s' в libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: КОНЕЦ ФАЙЛА при поиске последовательности заголовков\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не могу читать последовательность заголовков!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читать расширение последовательности заголовов!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Плохая последовательность заголовков!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Плохое расширение последовательности загловков!\n"

#define MSGTR_ShMemAllocFail "Не могу захватить общую память\n"
#define MSGTR_CantAllocAudioBuf "Не могу захватить выходной буффер аудио\n"

#define MSGTR_UnknownAudio "Неизвестный/потерянный аудио формат, отказ от звука\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Установка поддержки lirc...\n"
#define MSGTR_LIRCdisabled "Вы не сможете использовать Ваше удаленное управление\n"
#define MSGTR_LIRCopenfailed "Неудачное открытие поддержки lirc!\n"
#define MSGTR_LIRCcfgerr "Неудачное чтение файла конфигурации LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "О себе"
#define MSGTR_FileSelect "Выбрать файл ..."
#define MSGTR_SubtitleSelect "Выбрать субтитры ..."
#define MSGTR_OtherSelect "Выбор ..."
#define MSGTR_PlayList "PlayList"
#define MSGTR_SkinBrowser "Просмоторщик скинов"

// --- buttons ---
#define MSGTR_Ok "Да"
#define MSGTR_Cancel "Отмена"
#define MSGTR_Add "Добавить"
#define MSGTR_Remove "Удалить"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, не хватает памяти для отрисовки буффера."
#define MSGTR_NEMFMR "Sorry, не хватает памяти для отображения меню."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] ошибка в файле конфигурации скина на линии %d: %s" 
#define MSGTR_SKIN_WARNING1 "[skin] предупреждение: в файле конфигурации скина на линии %d: widget найден но до этого не найдена \"section\" ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] предупреждение: в файле конфигурации скина на линии %d: widget найден но до этого не найдена \"subsection\" (%s)"
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
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "обьявлено слишком много шрифтов\n"
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
#define MSGTR_MENU_SkinBrowser "Просмоторщик skin'ов"
#define MSGTR_MENU_Preferences "Настройки"
#define MSGTR_MENU_Exit "Выход ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "фатальная ошибка ..."
#define MSGTR_MSGBOX_LABEL_Error "ошибка ..."
#define MSGTR_MSGBOX_LABEL_Warning "предупреждение ..." 

#endif
