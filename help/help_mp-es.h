// Spanish translation by:
// Leandro Lucarella <leandro at lucarella.com.ar>,
// Jesús Climent <jesus.climent at hispalinux.es>,
// Sefanja Ruijsenaars <sefanja at gmx.net>,
// Andoni Zubimendi <andoni at lpsat.net>

// Updated to help_mp-en.h v1.126

// ========================= MPlayer help ===========================


#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Uso:   mplayer [opciones] [url o ruta del archivo]\n"
"\n"
"Opciones básicas: ('man mplayer' para una lista completa)\n"
" -vo <driver[:disp]>  Seleccionar driver de salida de vídeo y dispositivo ('-vo help' para obtener una lista).\n"
" -ao <driver[:disp]>  Seleccionar driver de salida de audio y dispositivo ('-ao help' para obtener una lista).\n"
#ifdef HAVE_VCD
" vcd://<numpista>      Reproducir pista de (S)VCD (Super Video CD) (acceso directo al dispositivo, no montado)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<número>        Reproducir título o pista de DVD desde un dispositivo en vez de un archivo regular.\n"
" -alang <lengua>      Seleccionar lengua para el audio del DVD (con código de país de dos caracteres. p. ej. 'es').\n"
" -alang <lengua>      Seleccionar lengua para los subtítulos del DVD.\n"
#endif
" -ss <tiempo>         Saltar a una posición determindada (en segundos o hh:mm:ss).\n"
" -nosound             No reproducir sonido.\n"
" -fs, -vm, -zoom      Opciones de pantalla completa (pantalla completa, cambio de modo de vídeo, escalado por software).\n"
" -x <x> -y <y>        Escalar imagen a resolución dada (para usar con -vm o -zoom).\n"
" -sub <archivo>       Especificar archivo de subtitulos a usar (mira también -subfps, -subdelay).\n"
" -playlist <archivo>  Especificar archivo con la lista de reproducción.\n"
" -vid <x> -aid <y>    Opciones para especificar el stream de vídeo (x) y de audio (y) a reproducir.\n"
" -fps <x> -srate <y>  Opciones para cambiar la tasa de vídeo (x fps) y de audio (y Hz).\n"
" -pp <calidad>        Activar filtro de postprocesado (lee la página man para más información).\n"
" -framedrop           Activar frame dropping (para máquinas lentas).\n\n"

"Teclas básicas ('man mplayer' para una lista completa, mira también input.conf):\n"
" <-  ó  ->       Avanzar o retroceder diez segundos.\n"
" arriba ó abajo  Avanzar o retroceder un minuto.\n"
" pgup ó pgdown   Avanzar o retroceder diez minutos.\n"
" < ó >           Avanzar o retroceder en la lista de reproducción.\n"
" p ó ESPACIO     Pausar el vídeo (presione cualquier tecla para continuar).\n"
" q ó ESC         Detener la reproducción y salir del programa.\n"
" + ó -           Ajustar el retardo de audio en más o menos 0.1 segundos.\n"
" o               Cambiar modo OSD:  nada / búsqueda / búsqueda + tiempo.\n"
" * ó /           Aumentar o disminuir el volumen (presione 'm' para elegir entre master/pcm).\n"
" z ó x           Ajustar el retardo de la subtítulación en más o menos 0.1 segundos.\n"
" r ó t           Ajustar posición de la subtítulación (mira también -vf expand).\n"
"\n"
" *** Lee la página man para más opciones (avanzadas) y teclas! ***\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nSaliendo...\n"
#define MSGTR_ExitingHow "\nSaliendo... (%s)\n"
#define MSGTR_Exit_quit "Salida."
#define MSGTR_Exit_eof "Fin del archivo."
#define MSGTR_Exit_error "Error fatal."
#define MSGTR_IntBySignal "\nMPlayer ha sido interrumpido por señal %d en el módulo: %s \n"
#define MSGTR_NoHomeDir "No se puede encontrar el directorio HOME.\n"
#define MSGTR_GetpathProblem "Problema en get_path(\"config\").\n"
#define MSGTR_CreatingCfgFile "Creando archivo de configuración: %s.\n"
#define MSGTR_InvalidAOdriver "Nombre del driver de salida de audio incorrecto: %s\nUsa '-ao help' para obtener la lista de drivers de salida de audio disponibles.\n"
#define MSGTR_CopyCodecsConf "Copia o enlaza <árbol del código fuente de MPlayer>/etc/codecs.conf a ~/.mplayer/codecs.conf\n"
#define MSGTR_BuiltinCodecsConf "Usando codecs.conf interno por defecto.\n" 
#define MSGTR_CantLoadFont "No se puede cargar fuente: %s.\n"
#define MSGTR_CantLoadSub "No se puede cargar la subtítulación: %s.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: No se encuentra el stream seleccionado.\n"
#define MSGTR_CantOpenDumpfile "No se puede abrir el archivo de dump.\n"
#define MSGTR_CoreDumped "Core dumped.\n"
#define MSGTR_FPSnotspecified "FPS no especificado (o inválido) en la cabecera! Usa la opción -fps.\n"
#define MSGTR_TryForceAudioFmtStr "Tratando de forzar la familia de codecs de audio %d...\n"
#define MSGTR_RTFMCodecs "Lea el archivo DOCS/HTML/es/codecs.html!\n"
#define MSGTR_CantFindAudioCodec "No se encuentra codec para el formato de audio 0x%X!\n"
#define MSGTR_TryForceVideoFmtStr "Tratando de forzar la familia de codecs de vídeo %d...\n"
#define MSGTR_CantFindVideoCodec "No se encuentra codec para el formato de vídeo 0x%X!\n"
#define MSGTR_CannotInitVO "FATAL: No se puede inicializar el driver de vídeo!\n"
#define MSGTR_CannotInitAO "No se puede abrir o inicializar dispositivo de audio, no se reproducirá sonido.\n"
#define MSGTR_StartPlaying "Empezando reproducción...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"       **************************************************************\n"\
"       **** Tu sistema es demasiado lento para reproducir esto!  ****\n"\
"       **************************************************************\n"\
"Posibles razones, problemas, soluciones:\n"\
"- Más común: driver de _audio_ con errores o roto.\n"\
"  - Intenta con -ao sdl, o usa ALSA 0.5 o la emulación OSS de ALSA 0.9.\n"\
"  - Proba con diferentes valores para -autosync, 30 es un buen comienzo.\n"\
"- Salida de vídeo lenta\n"\
"  - Proba otro driver -vo (para obtener una lista, -vo help) o intenta\n"\
"    iniciar con la opción -framedrop!\n"\
"- CPU lenta\n"\
"  - No intente reproducir DVDs o DivX grandes en una CPU lenta. Intenta\n"\
"    iniciar con la opción -hardframedrop.\n"\
"- Archivo erróneo o roto\n"\
"  - Proba combinaciones de -nobps -ni -mc 0.\n"\
"- Medios lentos (unidad NFS/SMB, DVD, VCD, etc)\n"\
"  - Intente con -cache 8192.\n"\
"- Esta usando -cache para reproducir archivos AVI no entrelazados?\n"\
"  - Intente con -nocache.\n"\
"Lea DOCS/HTML/es/video.html para consejos de ajuste o mayor velocidad.\n"\
"Si nada de eso sirve, lee DOCS/HTML/es/bugreports.html\n\n"

#define MSGTR_NoGui "MPlayer fue compilado sin soporte para interfaz gráfica.\n"
#define MSGTR_GuiNeedsX "La interfaz gráfica de MPlayer requiere X11!\n"
#define MSGTR_Playing "Reproduciendo %s.\n"
#define MSGTR_NoSound "Audio: sin sonido.\n"
#define MSGTR_FPSforced "FPS forzado en %5.3f  (ftime: %5.3f).\n"

#define MSGTR_CompiledWithRuntimeDetection "Compilado con detección en tiempo de ejecución de CPU - esto no es óptimo! Para obtener mejor rendimiento, recompila MPlayer con --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Compilado para CPU x86 con extensiones:"
#define MSGTR_AvailableVideoOutputDrivers "Drivers de salida de vídeo disponibles:\n"
#define MSGTR_AvailableAudioOutputDrivers "Drivers de salida de audio disponibles:\n"
#define MSGTR_AvailableAudioCodecs "Codecs de audio disponibles:\n"
#define MSGTR_AvailableVideoCodecs "Codecs de vídeo disponibles:\n"
#define MSGTR_AvailableAudioFm "\nFamilias/drivers de codecs de audio (compilados dentro de MPlayer) disponibles:\n"
#define MSGTR_AvailableVideoFm "\nFamilias/drivers de codecs de vídeo (compilados dentro de MPlayer) disponibles:\n"
#define MSGTR_AvailableFsType "Modos disponibles de cambio a pantalla completa:\n"
#define MSGTR_UsingRTCTiming "Usando el RTC timing por hardware de Linux (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Vídeo: no se puede leer las propiedades.\n"
#define MSGTR_NoStreamFound "No se ha encontrado stream.\n"
#define MSGTR_ErrorInitializingVODevice "Error abriendo/inicializando el dispositivo de la salida de vídeo (-vo)!\n"
#define MSGTR_ForcedVideoCodec "Forzado el codec de vídeo: %s.\n"
#define MSGTR_ForcedAudioCodec "Forzado el codec de audio: %s\n"
#define MSGTR_Video_NoVideo "Vídeo: no hay vídeo!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: No se pudieron inicializar los filtros de vídeo (-vf) o de salida de vídeo (-vo)!\n"
#define MSGTR_Paused "\n  =====  PAUSA  =====\r"
#define MSGTR_PlaylistLoadUnable "\nNo se puede cargar la lista de reproducción %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer se detuvo por una 'Instrucción Ilegal'.\n"\
"  Esto puede ser un defecto en nuestra rutina nueva de autodetección de CPU...\n"\
"  Por favor lee DOCS/HTML/es/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer se detuvo por una 'Instrucción Ilegal'.\n"\
"  Esto ocurre normalmente cuando ejecuta el programa en una CPU diferente de\n"\
"  la cual MPlayer fue compilado/optimizado.\n"\
"  ¡Verifica eso!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer se detuvo por mal uso de CPU/FPU/RAM.\n"\
"  Recompila MPlayer con la opción --enable-debug y hace un backtrace en\n"\
"  'gdb' y un desensamblado. Para más detalles, vea\n"\
"  DOCS/HTML/es/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer se detuvo. Esto no debería haber pasado.\n"\
"  Puede ser un defecto en el código de MPlayer _o_ en sus controladores\n"\
"  _o_ en su versión de gcc. Si piensa que es la culpa de MPlayer, por\n"\
"  favor lea DOCS/HTML/es/bugreports.html y siga las instrucciones que allí\n"\
"  se encuentran. No podemos y no lo ayudaremos a menos que nos provea esa\n"\
"  información cuando este reportando algún posible defecto.\n"

#define MSGTR_EdlCantUseBothModes "Imposible usar -edl y -edlout al mismo tiempo.\n"
#define MSGTR_EdlOutOfMem "No hay memoria suficiente para almacenar los datos EDL.\n"
#define MSGTR_EdlRecordsNo "Leidas %d acciones EDL.\n"
#define MSGTR_EdlQueueEmpty "No hay acciones EDL de las que ocuparse.\n"
#define MSGTR_EdlCantOpenForWrite "Error tratando de escribir en [%s].\n"
#define MSGTR_EdlCantOpenForRead "Error tratando de leer desde [%s].\n"
#define MSGTR_EdlNOsh_video "Imposible usar EDL sin video.\n"
#define MSGTR_EdlNOValidLine "Linea EDL inválida: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Ignorando linea EDL mal formateada [%d].\n"
#define MSGTR_EdlBadLineOverlap "Ultima posición de parada fue [%f]; próxima "\
"posición de partida es [%f]. Las operaciones deben estar en orden cronológico"\
", sin sobreponerse, ignorando.\n"
#define MSGTR_EdlBadLineBadStop "La posición de parada debe ser posterior a la"\
" posición de partida.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Usando el archivo de control pass3: %s\n"
#define MSGTR_MissingFilename "\nFalta el nombre del archivo.\n\n"
#define MSGTR_CannotOpenFile_Device "No se pudo abrir el archivo o el dispositivo.\n"
#define MSGTR_CannotOpenDemuxer "No se pudo abrir el demuxer.\n"
#define MSGTR_NoAudioEncoderSelected \
"\nNo se ha selecciono codificador de audio (-oac).\n"\
"  Escoge uno (mira -oac help) o usa -nosound.\n"
#define MSGTR_NoVideoEncoderSelected \
"\nNo se ha selecciono codificador de video (-ovc). \n"\
"  Escoge uno (mira -ovc help)\n"
#define MSGTR_CannotOpenOutputFile "No se puede abrir el archivo de salida'%s'.\n"
#define MSGTR_EncoderOpenFailed "No se pudo abrir el codificador.\n"
#define MSGTR_ForcingOutputFourcc "Forzando salida fourcc a %x [%.4s].\n"
#define MSGTR_WritingAVIHeader "Escribiendo cabecera AVI...\n"
#define MSGTR_DuplicateFrames "\n%d frame(s) duplicados.\n"
#define MSGTR_SkipFrame "\nse salta frame...\n"
#define MSGTR_ErrorWritingFile "%s: error escribiendo el archivo.\n"
#define MSGTR_WritingAVIIndex "\nEscribiendo index AVI...\n"
#define MSGTR_FixupAVIHeader "Arreglando cabecera AVI..\n"
#define MSGTR_RecommendedVideoBitrate "Bitrate recomendado para %s CD: %d.\n"
#define MSGTR_VideoStreamResult "\nStream de vídeo: %8.3f kbit/s (%d bps), tamaño: %d bytes, %5.3f segundos, %d frames\n"
#define MSGTR_AudioStreamResult "\nStream de audio: %8.3f kbit/s (%d bps), tamaño: %d bytes, %5.3f segundos\n"
#define MSGTR_OpenedStream "exito: formato: %d  datos: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "codec de video: copia de cuadro (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "codec de audio: copia de cuadro (formato=%x canales=%d razón=%ld bits=%d bps=%ld muestra-%ld)\n"
#define MSGTR_CBRPCMAudioSelected "audio PCM CBR seleccionado\n"
#define MSGTR_MP3AudioSelected "audio MP3 seleccionado\n"
#define MSGTR_CannotAllocateBytes "No se pueden asignar %d bytes\n"
#define MSGTR_SettingAudioDelay "Ajustando el RETRASO DEL AUDIO a %5.3f\n"
#define MSGTR_SettingAudioInputGain "Ajustando la ganancia de entrada de audio input a %f\n"
#define MSGTR_LamePresetEquals "\npreconfiguración=%s\n\n"
#define MSGTR_LimitingAudioPreload "Limitando la pre-carda de audio a 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Incrementando la densidad de audio a 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Forzando la precarga de audio a 0, puntos máximos de corrección ajustado a 0\n"
#define MSGTR_CBRAudioByterate "\n\naudio CBR: %ld bytes/seg, %d bytes/bloque\n"
#define MSGTR_LameVersion "versión de LAME %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Error: La tasa de bits especificada esta fuera de los rangos de valor para esta preconfiguración\n"\
"\n"\
"Cuando usa este modo debe ingresar un valor entre \"8\" y \"320\"\n"\
"\n"\
"Para mayor informción prueba: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Error: No ingreso un perfil valido y/o opciones con una preconfiguración\n"\
"\n"\
"Los perfiles disponibles son:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Modo ABR) - Implica el Modo ABR Mode. Para usarlo,\n"\
"                      solamente especifique la tasa de bits.. Por ejemplo:\n"\
"                      \"preset=185\" activates esta\n"\
"                      preconfiguración y usa 185 como el kbps promedio.\n"\
"\n"\
"    Algunos ejemplos:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" o  \"-lameopts  cbr:preset=192       \"\n"\
" o  \"-lameopts      preset=172       \"\n"\
" o  \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Para mayor información pruebe: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Las opciones de preconfiguración estan hechas para proveer la mayor calidad posible.\n"\
"\n"\
"Estas han sido en su mayor sometidas y ajustadas por medio de pruebas rigurosas de\n"\
"doble escucha ciega (double blind listening) para verificar y lograr este objetivo\n"\
"\n"\
"Son continuamente actualizadas para con el desarrollo actual que esta\n"\
"ocurriendo y como resultado debería proveer practicamente la mejor calidad\n"\
"actualmente posible de LAME.\n"\
"\n"\
"Para activar estas preconfiguración:\n"\
"\n"\
"   For modos VBR (en general mayor calidad):\n"\
"\n"\
"     \"preset=standard\" Esta preconfiguración generalmente debería ser transparente\n"\
"                             para la mayoría de la genta en la música y ya es bastante\n"\
"                             buena en calidad.\n"\
"\n"\
"     \"preset=extreme\" Si tiene un oido extramademente bueno y un equipo\n"\
"                             similar, esta preconfiguración normalmente le\n"\
"                             proveera una calidad levemente superior al modo "\
                               "\"standard\"\n"\
"\n"\
"   Para 320kbps CBR (la mayor calidad posible desde las preconfiguraciones):\n"\
"\n"\
"     \"preset=insane\"  Esta preconfiguración será excesiva para la mayoria\n"\
"                             de la gente en la mayoria de las ocasiones, pero si debe\n"\
"                             tener la mayor calidad posible sin tener en cuenta el\n"\
"                             tamaño del archivo, esta es la opción definitiva.\n"\
"\n"\
"   Para modos ABR (alta calidad por tasa de bits dado pero no tan alto como modo VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Usando esta preconfiguración normalmente obtendrá buena\n"\
"                             calidad a la tasa de bits especificada. Dependiendo de\n"\
"                             la tasa de bits ingresada, esta preconfiguración determinará\n"\
"                             las opciones optimas para esa situación particular.\n"\
"                             A pessar que funciona, no es tan flexible como el modo\n"\
"                             VBR, y normalmente no llegarán a obtener el mismo nivel\n"\
"                             de calidad del modo VBR a mayores tasas de bits.\n"\
"\n"\
"Las siguientes opciones también están disponibles para los correspondientes perfiles:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (Modo ABR) - Implica el Modo ABR Mode. Para usarlo,\n"\
"                      solamente especifique la tasa de bits.. Por ejemplo:\n"\
"                      \"preset=185\" activates esta\n"\
"                      preconfiguración y usa 185 como el kbps promedio.\n"\
"\n"\
"   \"fast\" - Activa el nuevo modo rápido VBR para un perfil en particular. La\n"\
"            desventaja al cambio de velocidad es que muchas veces la tasa de\n"\
"            bits será levemente más alta respecto del modo normal y la calidad\n"\
"            puede llegar a ser un poco más baja también.\n"\
"Advertencia: con la versión actual las preconfiruaciones \"fast\" puede llegar a\n"\
"             dar como resultado tasas de bits demasiado altas comparadas a los normales.\n"\
"\n"\
"   \"cbr\"  - Si usa el modo ABR (ver más arriba) con una tasa de bits significativa\n"\
"            como 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            puede usar la opción \"cbr\" para forzar la codificación en modo CBR\n"\
"            en lugar del modo por omisión ABR. ABR provee mayor calidad pero\n"\
"            CBR podría llegar a ser util en  situaciones tales como cuando se\n"\
"            recibe un flujo mp3 a través de internet.\n"\
"\n"\
"    Por ejemplo:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" o  \"-lameopts  cbr:preset=192       \"\n"\
" o  \"-lameopts      preset=172       \"\n"\
" o  \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Unos poco alias estan disponibles para el modo ABR:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_ConfigfileError "error en archivo de configuración"
#define MSGTR_ErrorParsingCommandLine "error en parametros de la línea de comando"
#define MSGTR_VideoStreamRequired "¡El flujo de video es obligatorio!\n"
#define MSGTR_ForcingInputFPS "en su lugar los cuadros por segundos de entrada serán interpretados como %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "El formato de archivo de salida RAWVIDEO no soporta audio - desactivando audio\n"
#define MSGTR_DemuxerDoesntSupportNosound "Este demuxer todavía no soporta -nosound.\n"
#define MSGTR_MemAllocFailed "falló la asignación de memoria"
#define MSGTR_NoMatchingFilter "¡No se encontró filtro o formato de salida concordante!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, ¿quiza este fallado el compilador de C?\n"
#define MSGTR_NoLavcAudioCodecName "LAVC Audio,¡falta el nombre del codec!\n"
#define MSGTR_LavcAudioCodecNotFound "LAVC Audio, no se encuentra el codificador para el codec %s\n"
#define MSGTR_CouldntAllocateLavcContext "LAVC Audio, ¡no se puede asignar contexto!\n"
#define MSGTR_CouldntOpenCodec "No se puede abrir el codec %s, br=%d\n"

// cfg-mencoder.h

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     método de tasa de bits variable\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           tasa de bits media\n"\
"\n"\
" cbr           tasa de bits constante\n"\
"               Forzar también modo de codificación CBR en modos ABR \n"\
"               preseleccionados subsecuentemente.\n"\
"\n"\
" br=<0-1024>   especifica tasa de bits en kBit (solo CBR y ABR)\n"\
"\n"\
" q=<0-9>       calidad (0-mejor, 9-peor) (solo para VBR)\n"\
"\n"\
" aq=<0-9>      calidad del algoritmo (0-mejor/lenta, 9-peor/rápida)\n"\
"\n"\
" ratio=<1-100> razón de compresión\n"\
"\n"\
" vol=<0-10>    configura ganancia de entrada de audio\n"\
"\n"\
" mode=<0-3>    (por defecto: auto)\n"\
"                0: estéreo\n"\
"                1: estéreo-junto\n"\
"                2: canal dual\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: no\n"\
"                1: todo\n"\
"                2: ajustar\n"\
"\n"\
" fast          Activa codificación rápida en modos VBR preseleccionados\n"\
"               subsecuentes, más baja calidad y tasas de bits más altas.\n"\
"\n"\
" preset=<value> Provee configuracion con la mejor calidad posible.\n"\
"                 medium: codificación VBR, buena calidad\n"\
"                 (rango de 150-180 kbps de tasa de bits)\n"\
"                 standard:  codificación VBR, alta calidad\n"\
"                 (rango de 170-210 kbps de tasa de bits)\n"\
"                 extreme: codificación VBR, muy alta calidad\n"\
"                 (rango de 200-240 kbps de tasa de bits)\n"\
"                 insane:  codificación CBR, la mejor calidad configurable\n"\
"                 (320 kbps de tasa de bits)\n"\
"                 <8-320>: codificación ABR con tasa de bits en promedio en los kbps dados.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo de CD-ROM '%s' no encontrado.\n"
#define MSGTR_ErrTrackSelect "Error seleccionando la pista de VCD!"
#define MSGTR_ReadSTDIN "Leyendo desde la entrada estándar (stdin)...\n"
#define MSGTR_UnableOpenURL "No se puede abrir URL: %s\n"
#define MSGTR_ConnToServer "Connectado al servidor: %s\n"
#define MSGTR_FileNotFound "Archivo no encontrado: '%s'\n"

#define MSGTR_SMBInitError "No se puede inicializar la librería libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "No se puede abrir desde la RED: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer no fue compilado con soporte de lectura de SMB.\n"

#define MSGTR_CantOpenDVD "No se puede abrir el dispositivo de DVD: %s\n"
#define MSGTR_DVDwait "Leyendo la estructura del disco, espere por favor...\n"
#define MSGTR_DVDnumTitles "Hay %d títulos en este DVD.\n"
#define MSGTR_DVDinvalidTitle "Número de título de DVD inválido: %d\n"
#define MSGTR_DVDnumChapters "Hay %d capítulos en este título de DVD.\n"
#define MSGTR_DVDinvalidChapter "Número de capítulo de DVD inválido: %d\n"
#define MSGTR_DVDnumAngles "Hay %d ángulos en este título de DVD.\n"
#define MSGTR_DVDinvalidAngle "Número de ángulo de DVD inválido: %d\n"
#define MSGTR_DVDnoIFO "No se puede abrir archivo IFO para el título de DVD %d.\n"
#define MSGTR_DVDnoVOBs "No se puede abrir VOBS del título (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD abierto.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Advertencia! Cabecera de stream de audio %d redefinida!\n"
#define MSGTR_VideoStreamRedefined "Advertencia! Cabecera de stream de video %d redefinida!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Demasiados (%d en %d bytes) paquetes de audio en el buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Demasiados (%d en %d bytes) paquetes de video en el buffer!\n"
#define MSGTR_MaybeNI "¿Estás reproduciendo un stream o archivo 'non-interleaved' o falló el codec?\n " \
		"Para archivos .AVI, intente forzar el modo 'non-interleaved' con la opción -ni.\n"
#define MSGTR_SwitchToNi "\nDetectado .AVI mal interleaveado - cambiando al modo -ni!\n"
#define MSGTR_Detected_XXX_FileFormat "Detectado formato de archivo %s.\n"
#define MSGTR_DetectedAudiofile "Detectado archivo de audio.\n"
#define MSGTR_NotSystemStream "Esto no es formato MPEG System Stream... (tal vez Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Stream MPEG-ES inválido? Contacta con el autor, podría ser un fallo.\n"
#define MSGTR_FormatNotRecognized "Este formato no está soportado o reconocido. Si este archivo es un AVI, ASF o MPEG, por favor contacte con el autor.\n"
#define MSGTR_MissingVideoStream "¡No se encontró stream de video!\n"
#define MSGTR_MissingAudioStream "No se encontró el stream de audio, no se reproducirá sonido.\n"
#define MSGTR_MissingVideoStreamBug "¡¿Stream de video perdido!? Contacta con el autor, podría ser un fallo.\n"

#define MSGTR_DoesntContainSelectedStream "demux: El archivo no contiene el stream de audio o video seleccionado.\n"

#define MSGTR_NI_Forced "Forzado"
#define MSGTR_NI_Detected "Detectado"
#define MSGTR_NI_Message "%s formato de AVI 'NON-INTERLEAVED'.\n"

#define MSGTR_UsingNINI "Usando formato de AVI roto 'NON-INTERLEAVED'.\n"
#define MSGTR_CouldntDetFNo "No se puede determinar el número de cuadros (para una búsqueda absoluta).\n"
#define MSGTR_CantSeekRawAVI "No se puede avanzar o retroceder en un stream crudo .AVI (se requiere índice, prueba con -idx).\n"
#define MSGTR_CantSeekFile "No se puede avanzar o retroceder en este archivo.\n"
#define MSGTR_EncryptedVOB "¡Archivo VOB encriptado! Lea DOCS/HTML/es/dvd.html.\n"
#define MSGTR_MOVcomprhdr "MOV: ¡Soporte de Cabecera comprimida requiere ZLIB!.\n"
#define MSGTR_MOVvariableFourCC "MOV: Advertencia. ¡Variable FOURCC detectada!\n"
#define MSGTR_MOVtooManyTrk "MOV: Advertencia. ¡Demasiadas pistas!"
#define MSGTR_FoundAudioStream "==> Encontrado stream de audio: %d\n"
#define MSGTR_FoundVideoStream "==> Encontrado stream de vîdeo: %d\n"
#define MSGTR_DetectedTV "Detectado TV.\n"
#define MSGTR_ErrorOpeningOGGDemuxer "No se puede abrir el demuxer ogg.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Buscando stream de audio (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "No se puede abrir stream de audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "No se puede abrir stream de subtítulos: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "No se pudo abrir el demuxer de audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "No se pudo abrir demuxer de subtítulos: %s\n"
#define MSGTR_TVInputNotSeekable "Entrada de TV no es buscable.\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Información de demuxer %s ya está disponible.\n"
#define MSGTR_ClipInfo "Información de clip: \n"
#define MSGTR_LeaveTelecineMode "\ndemux_mpg: contenido NTSC de 30cps detectado, cambiando cuadros por segundo.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: contenido NTSC progresivo de 24cps detectado, cambiando cuadros por segundo.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "No se pudo abrir codec.\n"
#define MSGTR_CantCloseCodec "No se pudo cerrar codec.\n"

#define MSGTR_MissingDLLcodec "ERROR: No se pudo abrir el codec DirectShow requerido: %s\n"
#define MSGTR_ACMiniterror "No se puede cargar/inicializar codecs de audio Win32/ACM (falta archivo DLL?)\n"
#define MSGTR_MissingLAVCcodec "No se encuentra codec '%s' en libavcodec...\n"


#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF mientras buscaba la cabecera de secuencia.\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: No se puede leer cabecera de secuencia.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: No se puede leer la extensión de la cabecera de secuencia.\n"
#define MSGTR_BadMpegSequHdr "MPEG: Mala cabecera de secuencia.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Mala extensión de la cabecera de secuencia.\n"

#define MSGTR_ShMemAllocFail "No se puede alocar memoria compartida.\n"
#define MSGTR_CantAllocAudioBuf "No se puede alocar buffer de la salida de audio.\n"
#define MSGTR_UnknownAudio "Formato de audio desconocido/perdido, no se reproducirá sonido.\n"


#define MSGTR_UsingExternalPP "[PP] Usando filtro de postprocesado externo, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Usando postprocesado del codec, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atributo de vídeo '%s' no es soportado por -vo y -vd actuales. \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Familia de codec de vídeo solicitada [%s] (vfm=%s) no está disponible (actívalo al compilar).\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Familia de codec de audio solicitada [%s] (afm=%s) no está disponible (actívalo al compilar).\n"
#define MSGTR_OpeningVideoDecoder "Abriendo descodificador de vídeo: [%s] %s.\n"
#define MSGTR_OpeningAudioDecoder "Abriendo descodificador de audio: [%s] %s.\n"
#define MSGTR_UninitVideoStr "uninit video: %s.\n"
#define MSGTR_UninitAudioStr "uninit audio: %s.\n"
#define MSGTR_VDecoderInitFailed "Inicialización del VDecoder ha fallado.\n"
#define MSGTR_ADecoderInitFailed "Inicialización del ADecoder ha fallado.\n"
#define MSGTR_ADecoderPreinitFailed "Preinicialización del ADecoder ha fallado.\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alocando %d bytes para el búfer de entrada.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Allocating %d + %d = %d bytes para el búfer de salida.\n"


// LIRC:
#define MSGTR_SettingUpLIRC "Configurando soporte para LIRC...\n"
#define MSGTR_LIRCdisabled "No podrás usar el control remoto.\n"
#define MSGTR_LIRCopenfailed "Fallo al abrir el soporte para LIRC.\n"
#define MSGTR_LIRCcfgerr "Fallo al leer archivo de configuración de LIRC %s.\n"


// vf.c
#define MSGTR_CouldNotFindVideoFilter "No se pudo encontrar el filtro de vídeo '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "No se pudo abrir el filtro de vídeo '%s'.\n"
#define MSGTR_OpeningVideoFilter "Abriendo filtro de vídeo: "
#define MSGTR_CannotFindColorspace "No se pudo encontrar espacio de color concordante, ni siquiera insertando 'scale' :(.\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: el codec no declaró sh->disp_w y sh->disp_h, intentando solucionarlo!\n"
#define MSGTR_VoConfigRequest "VDec: vo solicitud de config - %d x %d (csp preferida: %s).\n"
#define MSGTR_CouldNotFindColorspace "No se pudo encontrar colorspace concordante - reintentando escalado -vf...\n"
#define MSGTR_MovieAspectIsSet "Aspecto es %.2f:1 - prescalando a aspecto correcto.\n"
#define MSGTR_MovieAspectUndefined "Aspecto de película no es definido - no se ha aplicado prescalado.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Necesita actualizar/instalar el paquete binario con codecs.\n Dirijase a http://mplayerhq.hu/homepage/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Inicialización correcta de codec de vídeo Win32/DShow.\n"
#define MSGTR_DMOInitOK "INFO: Inicialización correcta de codec de vídeo Win32/DMO.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: ¡No se pudo enviar evento de pantalla completa EWMH!\n"

#define MSGTR_InsertingAfVolume "[Mixer] No mezclador de volumen por hardware, insertando filtro de volumen.\n"
#define MSGTR_NoVolume "[Mixer] Na hay control de volumen disponible.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Acerca de"
#define MSGTR_FileSelect "Seleccionar archivo..."
#define MSGTR_SubtitleSelect "Seleccionar subtítulos..."
#define MSGTR_OtherSelect "Seleccionar..."
#define MSGTR_AudioFileSelect "Seleccionar canal de audio externo..."
#define MSGTR_FontSelect "Seleccionar fuente..."
#define MSGTR_PlayList "Lista de reproducción"
#define MSGTR_Equalizer "Equalizador"
#define MSGTR_SkinBrowser "Navegador de skins"
#define MSGTR_Network "Streaming por red..."
#define MSGTR_Preferences "Preferencias"
#define MSGTR_AudioPreferences "Configuración de controlador de Audio"
#define MSGTR_NoMediaOpened "no se abrió audio/vídeo"
#define MSGTR_VCDTrack "pista VCD %d"
#define MSGTR_NoChapter "sin capítulo"
#define MSGTR_Chapter "capítulo %d"
#define MSGTR_NoFileLoaded "no se ha cargado ningún archivo"


// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Cancelar"
#define MSGTR_Add "Agregar"
#define MSGTR_Remove "Quitar"
#define MSGTR_Clear "Limpiar"
#define MSGTR_Config "Configurar"
#define MSGTR_ConfigDriver "Configurar driver"
#define MSGTR_Browse "Navegar"

// --- error messages ---
#define MSGTR_NEMDB "No hay suficiente memoria para dibujar el búfer."
#define MSGTR_NEMFMR "No hay suficiente memoria para dibujar el menú."
#define MSGTR_IDFGCVD "No se encuentra driver -vo compatible con la interfaz gráfica."
#define MSGTR_NEEDLAVCFAME "No puede reproducir archivos no MPEG con su DXR3/H+ sin recodificación. Activa lavc o fame en la configuración del DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] error en configuración de skin en la línea %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] advertencia en archivo de configuración en la línea %d:\n control (%s) encontrado pero no se encontro \"section\" antes"
#define MSGTR_SKIN_WARNING2 "[skin] advertencia en archivo de configuración en la línea %d:\n control (%s) encontrado pero no se encontro \"subsection\" antes"
#define MSGTR_SKIN_WARNING3 "[skin] advertencia en archivo de configuración en la linea %d:\nesta subsección no esta soportada por control (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Mapa de bits de 16 bits o menos no soportado (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Archivo no encontrado (%s).\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "Error al leer BMP (%s).\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "Error al leer TGA (%s).\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "Error al leer PNG (%s).\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed TGA no soportado (%s).\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "Tipo de archivo desconocido (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "Error de conversión de 24 bit a 32 bit (%s).\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "Mensaje desconocido: %s.\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "No hay suficiente memoria.\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Demasiadas fuentes declaradas.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Archivo de fuentes no encontrado.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Archivo de imagen de fuente noi encontrado.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificador de fuente no existente (%s).\n"
#define MSGTR_SKIN_UnknownParameter "parámetro desconocido (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin no encontrado (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Error de lectura del archivo de configuración del skin (%s).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Sobre MPlayer"
#define MSGTR_MENU_Open "Abrir..."
#define MSGTR_MENU_PlayFile "Reproducir archivo..."
#define MSGTR_MENU_PlayVCD "Reproducir VCD..."
#define MSGTR_MENU_PlayDVD "Reproducir DVD..."
#define MSGTR_MENU_PlayURL "Reproducir URL..."
#define MSGTR_MENU_LoadSubtitle "Cargar subtítulos..."
#define MSGTR_MENU_DropSubtitle "Cancelar subtitulos..."
#define MSGTR_MENU_LoadExternAudioFile "Cargar archivo de audio externo..."
#define MSGTR_MENU_Playing "Reproduciendo"
#define MSGTR_MENU_Play "Reproducir"
#define MSGTR_MENU_Pause "Pausa"
#define MSGTR_MENU_Stop "Parar"
#define MSGTR_MENU_NextStream "Siguiente stream"
#define MSGTR_MENU_PrevStream "Anterior stream"
#define MSGTR_MENU_Size "Tamaño"
#define MSGTR_MENU_NormalSize "Tamaño normal"
#define MSGTR_MENU_DoubleSize "Tamaño doble"
#define MSGTR_MENU_FullScreen "Pantalla completa"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Reproducir disco..."
#define MSGTR_MENU_ShowDVDMenu "Mostrar menú DVD"
#define MSGTR_MENU_Titles "Títulos"
#define MSGTR_MENU_Title "Título %2d"
#define MSGTR_MENU_None "(ninguno)"
#define MSGTR_MENU_Chapters "Capítulos"
#define MSGTR_MENU_Chapter "Capítulo %2d"
#define MSGTR_MENU_AudioLanguages "Idiomas de audio"
#define MSGTR_MENU_SubtitleLanguages "Idiomas de subtítulos"
#define MSGTR_MENU_PlayList "Lista de reproducción"
#define MSGTR_MENU_SkinBrowser "Navegador de skins"
#define MSGTR_MENU_Preferences "Preferencias"
#define MSGTR_MENU_Exit "Salir..."
#define MSGTR_MENU_Mute "Mudo"
#define MSGTR_MENU_Original "Original"
#define MSGTR_MENU_AspectRatio "Relación de aspecto"
#define MSGTR_MENU_AudioTrack "Pista de Audio"
#define MSGTR_MENU_Track "Pista %d"
#define MSGTR_MENU_VideoTrack "Pista de Video"


// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Contraste: "
#define MSGTR_EQU_Brightness "Brillo: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturación: "
#define MSGTR_EQU_Front_Left "Frente izquierdo"
#define MSGTR_EQU_Front_Right "Frente derecho"
#define MSGTR_EQU_Back_Left "Fondo izquierdo"
#define MSGTR_EQU_Back_Right "Fondo dercho"
#define MSGTR_EQU_Center "Centro"
#define MSGTR_EQU_Bass "Bajo"
#define MSGTR_EQU_All "Todos"
#define MSGTR_EQU_Channel1 "Canal 1:"
#define MSGTR_EQU_Channel2 "Canal 2:"
#define MSGTR_EQU_Channel3 "Canal 3:"
#define MSGTR_EQU_Channel4 "Canal 4:"
#define MSGTR_EQU_Channel5 "Canal 5:"
#define MSGTR_EQU_Channel6 "Canal 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Ubicación"
#define MSGTR_PLAYLIST_Selected "Archivos seleccionados"
#define MSGTR_PLAYLIST_Files "Archivos"
#define MSGTR_PLAYLIST_DirectoryTree "Árbol de directorios"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_Video "Vídeo"
#define MSGTR_PREFERENCES_SubtitleOSD "Subtítulos y OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs y demuxer"
#define MSGTR_PREFERENCES_Misc "Misc"

#define MSGTR_PREFERENCES_None "Ninguno"
#define MSGTR_PREFERENCES_DriverDefault "controlador por omisión"
#define MSGTR_PREFERENCES_AvailableDrivers "Drivers disponibles:"
#define MSGTR_PREFERENCES_DoNotPlaySound "No reproducir sonido"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizar sonido"
#define MSGTR_PREFERENCES_EnEqualizer "Activar equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Activar estereo extra"
#define MSGTR_PREFERENCES_Coefficient "Coeficiente:"
#define MSGTR_PREFERENCES_AudioDelay "Retraso de audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Activar buffering doble"
#define MSGTR_PREFERENCES_DirectRender "Activar renderización directa"
#define MSGTR_PREFERENCES_FrameDrop "Activar frame dropping"
#define MSGTR_PREFERENCES_HFrameDrop "Activar frame dropping DURO (peligroso)"
#define MSGTR_PREFERENCES_Flip "Visualizar imagen al revés"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timer e indicadores"
#define MSGTR_PREFERENCES_OSDProgress "Sólo barra de progreso"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, porcentaje y tiempo total"
#define MSGTR_PREFERENCES_Subtitle "Subtítulo:"
#define MSGTR_PREFERENCES_SUB_Delay "Retraso: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Posición: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Desactivar carga automática de subtítulos"
#define MSGTR_PREFERENCES_SUB_Unicode "Subtítulo unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Convertir el subtítulo dado al formato de subtítulos de MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Convertir el subtítulo dado al formato basado en tiempo SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Superposición de subtitulos"
#define MSGTR_PREFERENCES_Font "Fuente:"
#define MSGTR_PREFERENCES_FontFactor "Factor de fuente:"
#define MSGTR_PREFERENCES_PostProcess "Activar postprocesado"
#define MSGTR_PREFERENCES_AutoQuality "Calidad automática: "
#define MSGTR_PREFERENCES_NI "Usar non-interleaved AVI parser"
#define MSGTR_PREFERENCES_IDX "Reconstruir tabla de indices, si se necesita"
#define MSGTR_PREFERENCES_VideoCodecFamily "Familia de codec de vídeo:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Familia de codec de audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Nivel OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Subtítulos"
#define MSGTR_PREFERENCES_FRAME_Font "Fuente"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocesado"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec y demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_FRAME_Misc "Misc"
#define MSGTR_PREFERENCES_Audio_Device "Dispositivo:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mezclador:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Canal del Mezclador:"
#define MSGTR_PREFERENCES_Message "Algunas opciones requieren reiniciar la reproducción."
#define MSGTR_PREFERENCES_DXR3_VENC "Codificador de vídeo:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Usar LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Usar FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Occidental (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Occidental con euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Eslavo/Centroeuropeo (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Gallego, Maltés, Turco (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Báltico (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cirílico (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Árabe (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Griego moderno (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turco (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Báltico (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Céltico (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebreo (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Ruso (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Belaruso (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Chino simplificado (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Chino tradicional (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japanés(SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Coreano (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cirílico (Windows) (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Eslavo/Centroeuropeo (Windows) (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Sin autoescalado"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcional a la anchura de película"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcional a la altura de película"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcional al diagonal de película"
#define MSGTR_PREFERENCES_FontEncoding "Codificación:"
#define MSGTR_PREFERENCES_FontBlur "Blur:"
#define MSGTR_PREFERENCES_FontOutLine "Outline:"
#define MSGTR_PREFERENCES_FontTextScale "Escalado de texto:"
#define MSGTR_PREFERENCES_FontOSDScale "Escalado de OSD:"
#define MSGTR_PREFERENCES_SubtitleOSD "Subtítulos y OSD"
#define MSGTR_PREFERENCES_Cache "Cache si/no"
#define MSGTR_PREFERENCES_CacheSize "Tamaño de Cache: "
#define MSGTR_PREFERENCES_LoadFullscreen "Empezar en pantalla completa"
#define MSGTR_PREFERENCES_SaveWinPos "Guardar posición de la ventana"
#define MSGTR_PREFERENCES_XSCREENSAVER "Detener Salvador de Pantallas de X"
#define MSGTR_PREFERENCES_PlayBar "Habilitar barra de reproducción"
#define MSGTR_PREFERENCES_AutoSync "AutoSync si/no"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "Dispositivo de CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Dispositivo de DVD:"
#define MSGTR_PREFERENCES_FPS "Cuadros por segundo de la Pelicula:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Mostrar Ventana de Video cuando este inactiva"

#define MSGTR_ABOUT_UHU " Desarrollo de GUI patrocinado por UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   Equipo principal de MPlayer:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Otros programadores:\n"
#define MSGTR_ABOUT_MainTesters "   Testeadores más importantes:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Error fatal"
#define MSGTR_MSGBOX_LABEL_Error "Error"
#define MSGTR_MSGBOX_LABEL_Warning "Advertencia"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Disculpe, el dispositivo de salida de vídeo es incompatible con este codec.\n"
#define MSGTR_VO_GenericError "Este error ha ocurrido"
#define MSGTR_VO_UnableToAccess "No es posible acceder"
#define MSGTR_VO_ExistsButNoDirectory "ya existe, pero no es un directorio."
#define MSGTR_VO_DirExistsButNotWritable "El directorio ya existe, pero no se puede escribir en él."
#define MSGTR_VO_DirExistsAndIsWritable "El directorio ya existe y se puede escribir en él."
#define MSGTR_VO_CantCreateDirectory "No es posible crear el directorio de salida."
#define MSGTR_VO_DirectoryCreateSuccess "Directorio de salida creado exitosamente."

// vo_jpeg.c - desde aqui hasta abajo se ha sincronizado con help_mp-en (1.148)
#define MSGTR_VO_JPEG_ProgressiveJPEG "JPEG progresivo habilitado."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "JPEG progresivo deshabilitado."
#define MSGTR_VO_JPEG_BaselineJPEG "Baseline JPEG habilitado."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline JPEG deshabilitado."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "modo ASCII habilitado."
#define MSGTR_VO_PNM_RawMode "Raw mode habilitado."
#define MSGTR_VO_PNM_PPMType "Escribiré archivos PPM."
#define MSGTR_VO_PNM_PGMType "Escribiré archivos PGM."
#define MSGTR_VO_PNM_PGMYUVType "Escribiré archivos PGMYUV."
 
// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Modo interlaceado requiere que la altura de la imagen sea divisible por 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "No se pudo reservar buffer de línea para el modo interlaceado."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Entrada no es RGB, imposible separar crominancia por campos!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "El ancho de la imagen debe ser divisible por 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "No hay memoria suficiente para reservar framebuffer RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Imposible obtener memoria o descriptor de archivos para escribir \"stream.yuv\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Error escribiendo imagen a la salida!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Se desconoce el subdevice: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Usando modo de salida interlaceado, top-field primero."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Usando modo de salida interlaceado, bottom-field primero."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Usando (por defecto) modo de cuadro progresivo."

// Viejos vo drivers que han sido reemplazados

#define MSGTR_VO_PGM_HasBeenReplaced "El controlador de salida pgm ha sido reemplazado por -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "El controlador de salida md5 ha sido reemplazado por -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: los módulos alsa9 y alsa1x fueron eliminados, use -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Imposible abrir dispositivo mezclador %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: El mezclador de la tarjeta de audio no tiene el canal '%s' usando valor por omisión.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Imposible abrir dispositivo de audio %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Imposible crear descriptor de archivo, bloqueando: %s\n"
#define MSGTR_AO_OSS_CantSetAC3 "[AO OSS] Imposible configurar dispositivo de audio %s a salida AC3, tratando S16...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Imposible configurar dispositivo de audio a %d channels.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: El controlador no soporta SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Su controlador de audio no soporta select()  ***\n Recompile MPlayer con #undef HAVE_AUDIO_SELECT en config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\n Error fatal: *** Imposible RE-ABRIR / RESETEAR DISPOSITIVO DE AUDIO *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Conectado al servidor de sonido.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Imposible abrir stream.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Stream Abierto.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] Tamaño del buffer: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Fallo Seteando volumen en %d.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz no soportado, trate \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] Fallo en esd_open_sound: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latencia: [servidor: %0.2fs, red: %0.2fs] (ajuste %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Fallo abriendo playback stream esd: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Fallo configurando mezclador de audio DVB:%s\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz no soportado, trate de resamplear...\n"
 
// ao_null.c

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Archivo: %s (%s)\nPCM: Samplerate: %iHz Canales: %s Formato %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: El volcado más rápido se logra con -vc dummy -vo null\nPCM: Info: Para escribir archivos de onda (WAVE) use -waveheader (valor por omisión).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Imposible abrir %s para escribir!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Canales: %s Formato %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] usando controlador de audio: %s .\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Formato de audio no soportado: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Error inicializando audio SDL: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Imposible abrir audio: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] control.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Canales: %s Formato %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: dispositivo inválido.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: fallo en setparams: %s\nImposble configurar samplerate deseado.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE No fue aceptado en el recurso dado.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: fallo en getparams: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: samplerate es ahora %lf (el deseado es %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Imposible abrir canal de audio: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
 #define MSGTR_AO_SGI_Reset "[AO SGI] reseteando: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pausa: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_continuar: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: Fallo en SETINFO.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: Fallo escribiendo."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Imposible abrir dispositivo de audio %s, %s -> nosound.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Su tarjeta no soporta el canal %d, %s, %d Hz samplerate.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Su controlador de audio no soporta select()  ***\nRecompile MPlayer con #undef HAVE_AUDIO_SELECT en config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nError fatal: *** IMPOSIBLE RE-ABRIR / RESETEAR DISPOSITIVO DE AUDIO (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: formato solicitado: %d Hz, %d canales, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: No se encontró tarjeta de sonido alguna.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: formato inválido (%s) solicitado - deshabilitando salida.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: Fallo en playback open: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: Error en pcm info: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d tarjeta(s) de sonido encontrada(s), using: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: Error en la información del canal PCM: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: Error configurando parámetros: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: Error configurando canal: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: Error preparando canal: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: Error de 'playback drain': %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: Error de 'playback flush': %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: Error cerrando pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: Error de 'playback drain': %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: Error de 'playback flush': %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: Error preparando canal: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: Error de 'playback drain': %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: Error de 'playback flush': %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: Error preparando canal: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: alsa underrun, reseteando stream.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: Error preparando playback: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: Error de escritura despues de resetear: %s - me rindo!.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: Error de salida: %s\n"

// ao_plugin.c

 #define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] Plugin inválido: %s\n"
