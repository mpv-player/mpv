/* Translated by:  Volodymyr M. Lisivka <lvm@mystery.lviv.net>
   Was synced with help_mp-en.h: rev 1.20
 ========================= MPlayer help =========================== */

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (див. DOCS!)\n"
"\n";

static char help_text[]=
"Запуск:   mplayer [опц╕╖] [path/]filename\n"
"\n"
"Опц╕╖:\n"
" -vo <drv[:dev]> виб╕р драйвера ╕ пристрою в╕део виводу (список див. з '-vo help')\n"
" -ao <drv[:dev]> виб╕р драйвера ╕ пристрою ауд╕о виводу (список див. з '-ao help')\n"
" -vcd <номер треку> грати VCD (video cd) трек з пристрою зам╕сть файлу\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  виб╕р пристрою DVD для авторизац╕╖ (для шифрованих диск╕в)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <номер титр╕в> грати DVD титри/трек з пристрою зам╕сть файлу\n"
#endif
" -ss <час>     перем╕ститися на задану (секунди або ГГ:ММ:СС) позиц╕ю\n"
" -nosound        без звуку\n"
#ifdef USE_FAKE_MONO
" -stereo <режим> виб╕р MPEG1 стерео виводу (0:стерео 1:л╕вий 2:правий)\n"
#endif
" -channels <n>   номер вих╕дних канал╕в звуку\n"
" -fs -vm -zoom   опц╕╖ повноекранного програвання (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   маштабувати картинку до <x> * <y> [якщо -vo драйвер п╕дтриму╓!]\n"
" -sub <file>     вказати файл субтитр╕в (див. також -subfps, -subdelay)\n"
" -playlist <file> вказати playlist\n"
" -vid x -aid y   опц╕╖ для вибору в╕део (x) ╕ ауд╕о (y) потоку для програвання\n"
" -fps x -srate y опц╕╖ для зм╕ни в╕део (x кадр/сек) ╕ ауд╕о (y Hz) швидкост╕\n"
" -pp <quality>   дозволити ф╕льтр (0-4 для DivX, 0-63 для mpegs)\n"
" -nobps          використовувати альтернативний метод синхрон╕зац╕╖ A-V для AVI файл╕в (може допомогти!)\n"
" -framedrop      дозволити втрату кадр╕в (для пов╕льних машин)\n"
" -wid <id в╕кна>  використовувати ╕снуюче в╕кно для в╕део виводу (корисно для plugger!)\n"
"\n"
"Клав╕ш╕:\n"
" <-  або ->      перемотування вперед/назад на 10 секунд\n"
" вверх або вниз  перемотування вперед/назад на  1 хвилину\n"
" < або >         перемотування вперед/назад у списку програвання\n"
" p або ПРОБ╤Л    зупинити ф╕льм (будь-яка клав╕ша - продовжити)\n"
" q або ESC       зупинити в╕дтворення ╕ вих╕д\n"
" + або -         регулювати затримку звуку по +/- 0.1 секунд╕\n"
" o               цикл╕чний переб╕р OSD режим╕в:  нема / нав╕гац╕я / нав╕гац╕я+таймер\n"
" * або /         додати або зменшити гучн╕сть (натискання 'm' вибира╓ master/pcm)\n"
" z або x         регулювати затримку субтитр╕в по +/- 0.1 секунд╕\n"
"\n"
" * * * ДЕТАЛЬН╤ШЕ ДИВ. ДОКУМЕНТАЦ╤Ю, ПРО ДОДАТКОВ╤ ОПЦ╤╥ ╤ КЛЮЧ╤ ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nВиходимо... (%s)\n"
#define MSGTR_Exit_frames "Запитана к╕льк╕сть кадр╕в програна"
#define MSGTR_Exit_quit "Вих╕д"
#define MSGTR_Exit_eof "К╕нець файлу"
#define MSGTR_Exit_error "Фатальна помилка"
#define MSGTR_IntBySignal "\nMPlayer перерваний сигналом %d у модул╕: %s \n"
#define MSGTR_NoHomeDir "Не можу знайти домашн╕й каталог\n"
#define MSGTR_GetpathProblem "проблеми у get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Створення файлу конф╕гурац╕╖: %s\n"
#define MSGTR_InvalidVOdriver "Неприпустиме ╕м'я драйверу в╕део виводу: %s\nДив. '-vo help' щоб отримати список доступних драйвер╕в.\n"
#define MSGTR_InvalidAOdriver "Неприпустиме ╕м'я драйверу ауд╕о виводу: %s\nДив. '-ao help' щоб отримати список доступних драйвер╕в.\n"
#define MSGTR_CopyCodecsConf "(скоп╕юйте etc/codecs.conf (з текст╕в MPlayer) у ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Не можу завантажити шрифт: %s\n"
#define MSGTR_CantLoadSub "Не можу завантажити субтитри: %s\n"
#define MSGTR_ErrorDVDkey "Помилка обробки DVD КЛЮЧА.\n"
#define MSGTR_CmdlineDVDkey "Командний рядок DVD вимага╓ записаний ключ для дешифрування.\n"
#define MSGTR_DVDauthOk "Авторизац╕я DVD - все гаразд.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: обраний пот╕к загублений!\n"
#define MSGTR_CantOpenDumpfile "Не можу в╕дкрити файл дампу!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "К╕льк╕сть кадр╕в на секунду не вказано (або неприпустиме значення) у заголовку! Використовуйте ключ -fps!\n"
#define MSGTR_NoVideoStream "В╕део пот╕к не знайдений... це поки що не програ╓ться\n"
#define MSGTR_TryForceAudioFmt "Спроба примусово використати с╕мейство ауд╕о кодек╕в %d ...\n"
#define MSGTR_CantFindAfmtFallback "Не можу знайти ауд╕о кодек для вказаного с╕мейства, перех╕д на ╕нш╕ драйвери.\n"
#define MSGTR_CantFindAudioCodec "Не можу знайти кодек для ауд╕о формату 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Спробуйте оновити %s з etc/codecs.conf\n*** Якщо не допомогло - читайте DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Не зм╕г ╕н╕ц╕ал╕зувати ауд╕о кодек! -> граю без звуку\n"
#define MSGTR_TryForceVideoFmt "Спроба примусово використати с╕мейство в╕део кодек╕в %d ...\n"
#define MSGTR_CantFindVfmtFallback "Не можу знайти в╕део кодек для вказаного с╕мейства, перех╕д на ╕нш╕ драйвери.\n"
#define MSGTR_CantFindVideoCodec "Не можу знайти кодек для в╕део формату 0x%X !\n"
#define MSGTR_VOincompCodec "Вибачте, обраний video_out пристр╕й не сум╕сний з цим кодеком.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Не зм╕г ╕н╕ц╕ал╕зувати в╕део кодек :(\n"
#define MSGTR_EncodeFileExists "Файл вже ╕сну╓: %s (не перезаписуйте ваш улюблений AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Не можу створити файл для кодування\n"
#define MSGTR_CannotInitVO "FATAL: Не можу ╕н╕ц╕ал╕зувати в╕део драйвер!\n"
#define MSGTR_CannotInitAO "не можу в╕дкрити/╕н╕ц╕ал╕зувати ауд╕о пристр╕й -> ГРАЮ БЕЗ ЗВУКУ\n"
#define MSGTR_StartPlaying "Початок програвання...\n"
#define MSGTR_SystemTooSlow "\n\n"\
"         *****************************************************************\n"\
"         **** Ваша система надто ПОВ╤ЛЬНА щоб в╕дтворити це! ****\n"\
"         *****************************************************************\n"\
"!!! Можлив╕ причини, проблеми, обх╕дн╕ шляхи: \n"\
"- Найб╕льш загальн╕: поганий/сирий _ауд╕о_ драйвер : спробуйте -ao sdl або\n"\
"  використовуйте ALSA 0.5 або емуляц╕ю oss на ALSA 0.9. Читайте DOCS/sound.html!\n"\
"- Пов╕льний в╕део вив╕д. Спробуйте ╕нший -vo драйвер (список: -vo help) або\n"\
"  спробуйте з -framedrop ! Читайте DOCS/video.html.\n"\
"- Пов╕льний ЦП. Не намагайтеся в╕дтворювати велик╕ dvd/divx на пов╕льних\n"\
"  процесорах! спробуйте -hardframedrop\n"\
"- Битий файл. Спробуйте р╕зн╕ комб╕нац╕╖: -nobps  -ni  -mc 0  -forceidx\n"\
"Якщо н╕чого не допомогло, тод╕ читайте DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer був скомп╕льований БЕЗ п╕дтримки GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI вимага╓ X11!\n"
#define MSGTR_Playing "Програвання %s\n"
#define MSGTR_NoSound "Ауд╕о: без звуку!!!\n"
#define MSGTR_FPSforced "Примусово зм╕нена к╕льк╕сть кадр╕в на секунду на %5.3f (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Компактов╕д \"%s\" не знайдений!\n"
#define MSGTR_ErrTrackSelect "Помилка вибору треку на VCD!"
#define MSGTR_ReadSTDIN "Читання з stdin...\n"
#define MSGTR_UnableOpenURL "Не можу в╕дкрити URL: %s\n"
#define MSGTR_ConnToServer "З'╓днання з сервером: %s\n"
#define MSGTR_FileNotFound "Файл не знайдений: '%s'\n"

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
#define MSGTR_MaybeNI "(можливо ви програ╓те нечерезрядковий пот╕к/файл або невдалий кодек)\n"
#define MSGTR_DetectedFILMfile "Знайдений FILM формат файлу!\n"
#define MSGTR_DetectedFLIfile "Знайдений FLI формат файлу!\n"
#define MSGTR_DetectedROQfile "Знайдений RoQ формат файлу!\n"
#define MSGTR_DetectedREALfile "Знайдений REAL формат файлу!\n"
#define MSGTR_DetectedAVIfile "Знайдений AVI формат файлу!\n"
#define MSGTR_DetectedASFfile "Знайдений ASF формат файлу!\n"
#define MSGTR_DetectedMPEGPESfile "Знайдений MPEG-PES формат файлу!\n"
#define MSGTR_DetectedMPEGPSfile "Знайдений MPEG-PS формат файлу!\n"
#define MSGTR_DetectedMPEGESfile "Знайдений MPEG-ES формат файлу!\n"
#define MSGTR_DetectedQTMOVfile "Знайдений QuickTime/MOV формат файлу!\n"
#define MSGTR_MissingMpegVideo "MPEG в╕део пот╕к загублений!? Зв'яж╕ться з автором, це мабуть помилка :(\n"
#define MSGTR_InvalidMPEGES "Неприпустимий MPEG-ES пот╕к??? Зв'яж╕ться з автором, це мабуть помилка :(\n"
#define MSGTR_FormatNotRecognized "========= Вибачте, формат цього файлу не розп╕знаний чи не п╕дтриму╓ться ===========\n"\
				  "===== Якщо це AVI, ASF або MPEG пот╕к, будь ласка зв'яж╕ться з автором! ======\n"
#define MSGTR_MissingVideoStream "В╕део пот╕к не знайдений!\n"
#define MSGTR_MissingAudioStream "Ауд╕о пот╕к не знайдений...  -> програю без звуку\n"
#define MSGTR_MissingVideoStreamBug "В╕део пот╕к загублений!? Зв'яж╕ться з автором, це мабуть помилка :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: файл не м╕стить обраний ауд╕о або в╕део пот╕к\n"

#define MSGTR_NI_Forced "Примусово вибраний"
#define MSGTR_NI_Detected "Знайдений"
#define MSGTR_NI_Message "%s НЕЧЕРЕЗРЯДКОВИЙ формат AVI файлу!\n"

#define MSGTR_UsingNINI "Використання НЕЧЕРЕЗРЯДКОВОГО або пошкодженого формату AVI файлу!\n"
#define MSGTR_CouldntDetFNo "Не зм╕г визначити число кадр╕в (для абсолютного перенесення)\n"
#define MSGTR_CantSeekRawAVI "Не можу перем╕ститися у непро╕ндексованому потоц╕ .AVI! (вимага╓ться ╕ндекс, спробуйте з ключом -idx!)\n"
#define MSGTR_CantSeekFile "Не можу перем╕щуватися у цьому файл╕!\n"

#define MSGTR_EncryptedVOB "Шифрований VOB файл (mplayer не скомп╕льований з п╕дтримкою libcss)! Див. DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Шифрований пот╕к але ви не вимагали авторизац╕╖!!\n"

#define MSGTR_MOVcomprhdr "MOV: Стиснут╕ заголовки (поки що) не п╕дтримуються!\n"
#define MSGTR_MOVvariableFourCC "MOV: Попередження! Знайдено перем╕нний FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Попередження! надто багато трек╕в!"
#define MSGTR_MOVnotyetsupp "\n****** Формат Quicktime MOV поки не п╕дтриму╓ться!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Не зм╕г в╕дкрити кодек\n"
#define MSGTR_CantCloseCodec "Не зм╕г закрити кодек\n"

#define MSGTR_MissingDLLcodec "ПОМИЛКА: Не зм╕г в╕дкрити необх╕дний DirectShow кодек: %s\n"
#define MSGTR_ACMiniterror "Не зм╕г завантажити чи ╕н╕ц╕ал╕зувати Win32/ACM AUDIO кодек (загублений DLL файл?)\n"
#define MSGTR_MissingLAVCcodec "Не можу знайти кодек \"%s\" у libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer був скомп╕льований БЕЗ п╕дтримки directshow!\n"
#define MSGTR_NoWfvSupport "П╕дтримка для win32 кодек╕в заборонена або недоступна на не-x86 платформах!\n"
#define MSGTR_NoDivx4Support "MPlayer був скомп╕льований БЕЗ п╕дтримки DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer був скомп╕льований БЕЗ п╕дтримки ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM ауд╕о кодек заборонений, або недоступний на не-x86 ЦП -> заблокуйте звук :(\n"
#define MSGTR_NoDShowAudio "MPlayer був скомп╕льований без п╕дтримки DirectShow -> заблокуйте звук :(\n"
#define MSGTR_NoOggVorbis "OggVorbis ауд╕о кодек заборонений -> заблокуйте звук :(\n"
#define MSGTR_NoXAnimSupport "MPlayer був скомп╕льований БЕЗ п╕дтримки XAnim!\n"

#define MSGTR_MpegPPhint "ПОПЕРЕДЖЕННЯ! Ви запитали ф╕льтрування для MPEG 1/2 в╕део,\n" \
			 "         але скомп╕лювали MPlayer без п╕дтримки ф╕льтр╕в для MPEG 1/2!\n" \
			 "         #define MPEG12_POSTPROC у config.h, ╕ перекомп╕люйте libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: К╤НЕЦЬ ФАЙЛУ при пошуку посл╕довност╕ заголовк╕в\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Не можу читати посл╕довн╕сть заголовк╕в!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Не мочу читати розширення посл╕довност╕ заголовк╕в!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Погана посл╕довн╕сть заголовк╕в!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Погане розширення посл╕довност╕ заголовк╕в!\n"

#define MSGTR_ShMemAllocFail "Не можу захопити загальну пам'ять\n"
#define MSGTR_CantAllocAudioBuf "Не можу захопити вих╕дний буфер ауд╕о\n"
#define MSGTR_NoMemForDecodedImage "Не досить пам'ят╕ для буфера декодування картинки (%ld байт)\n"

#define MSGTR_AC3notvalid "Не припустимий AC3 пот╕к.\n"
#define MSGTR_AC3only48k "П╕дтриму╓ться лише пот╕к з частотою 48000 Hz.\n"
#define MSGTR_UnknownAudio "Нев╕домий чи загублений ауд╕о формат, програю без звуку\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Встановлення п╕дтримки lirc...\n"
#define MSGTR_LIRCdisabled "Ви не зможете використовувати ваше в╕ддалене керування\n"
#define MSGTR_LIRCopenfailed "Невдале в╕дкриття п╕дтримки lirc!\n"
#define MSGTR_LIRCsocketerr "Щось негаразд з гн╕здом lirc: %s\n"
#define MSGTR_LIRCcfgerr "Невдале читання файлу конф╕гурац╕╖ LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Про програму"
#define MSGTR_FileSelect "Вибрати файл ..."
#define MSGTR_SubtitleSelect "Вибрати субтитри ..."
#define MSGTR_OtherSelect "Виб╕р ..."
#define MSGTR_MessageBox "Пов╕домлення"
#define MSGTR_PlayList "Список програвання"
#define MSGTR_SkinBrowser "Переглядач жупан╕в"

// --- buttons ---
#define MSGTR_Ok "Так"
#define MSGTR_Cancel "Скасувати"
#define MSGTR_Add "Додати"
#define MSGTR_Remove "Видалити"

// --- error messages ---
#define MSGTR_NEMDB "Вибачте, не вистача╓ пам'ят╕ для в╕дмальовування буферу."
#define MSGTR_NEMFMR "Вибачте, не вистача╓ пам'ят╕ для в╕дображення меню."
#define MSGTR_NEMFMM "Вибачте, не вистача╓ пам'ят╕ для маски форми головного в╕кна."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[жупан] помилка у файл╕ конф╕гурац╕╖ жупана, рядок %d  : %s" 
#define MSGTR_SKIN_WARNING1 "[жупан] попередження: у файл╕ конф╕гурац╕╖ жупана, рядок %d: widget знайдений але до цього не знайдено \"section\" ( %s )"
#define MSGTR_SKIN_WARNING2 "[жупан] попередження: у файл╕ конф╕гурац╕╖ жупана, рядок %d: widget знайдений але до цього не знайдено \"subsection\" (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Глибина кольору б╕тово╖ карти у 16 б╕т ╕ менше не п╕дтриму╓ться ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "файл не знайдений ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "помилка читання bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "помилка читання tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "помилка читання png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE запакований tga не п╕дтриму╓ться ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "нев╕домий тип файлу ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "помилка перетворення 24-б╕т у 32-б╕т ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "нев╕доме пов╕домлення: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "не вистача╓ пам'ят╕\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "оголошено надто багато шрифт╕в\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "файл шрифту не знайдений\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "файл образ╕в шрифту не знайдений\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "не╕снуючий ╕дентиф╕катор шрифту ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "нев╕домий параметр ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[переглядач жупан╕в] не вистача╓ пам'ят╕.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Жупан не знайдено ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Помилка читання файла конф╕гурац╕╖ жупана ( %s ).\n"
#define MSGTR_SKIN_LABEL "Жупани:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Про програму"
#define MSGTR_MENU_Open "В╕дкрити ..."
#define MSGTR_MENU_PlayFile "Грати файл ..."
#define MSGTR_MENU_PlayVCD "Грати VCD ..."
#define MSGTR_MENU_PlayDVD "Грати DVD ..."
#define MSGTR_MENU_PlayURL "Грати URL ..."
#define MSGTR_MENU_LoadSubtitle "Завантажити субтитри ..."
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
#define MSGTR_MENU_PlayDisc "Грати диск ..."
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
#define MSGTR_MENU_Exit "Вих╕д ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "фатальна помилка ..."
#define MSGTR_MSGBOX_LABEL_Error "помилка ..."
#define MSGTR_MSGBOX_LABEL_Warning "попередження ..." 

#endif
