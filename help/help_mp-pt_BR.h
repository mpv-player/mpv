// Translated by Fabio Pugliese Ornellas <fabio.ornellas@poli.usp.br>
// Portuguese from Brazil Translation
// GPLed code
// in sync version 1.87 from CVS 2002-02-04

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Uso:   mplayer [opções] [url|caminho/]nome-do-arquivo\n"
"\n"
"Opções básicas: (lista completa na página do manual)\n"
" -vo <drv[:dev]> seleciona o driver de saída de vídeo & dispositivo\n"
"                 ('-vo help' para listar)\n"
" -ao <drv[:dev]> seleciona o driver de saída de audio & dispositivo\n"
"                 ('-vo help' para listar)\n"
#ifdef HAVE_VCD
" vcd://<numtrilha> reproduz trilha de VCD (Video CD) do dispositivo em vez de um\n"
"                 arquivo\n"
#endif
#ifdef USE_DVDREAD
" dvd://<numtítilo> reproduz título de DVD do dispositivo em vez de um arquivo\n"
" -alang/-slang   seleciona o idioma/legenda do DVD (pelo código país de duas\n"
"                 letras)\n"
#endif
" -ss <tempopos>  busca para a posição dada (segundos ou hh:mm:ss)\n"
" -nosound        não reproduz som\n"
" -fs             reprodução em tela cheia (ou -vm, -zoom, detalhes na página do\n"
"                 manual)\n"
" -x <x> -y <y>   especifica a resolução da tela (para uso com -vm ou -zoom)\n"
" -sub <arquivo>  especifica o arquivo de legenda a usar (veja também -subfps,\n"
"                 -subdelay)\n"
" -playlist <arquivo> especifica o aruqivo com a lista de reprodução\n"
" -vid x -aid y   seleciona a trilha de vídeo (x) e audio (y) a reproduzir\n"
" -fps x -srate y muda a taxa do vídeo (x quadros por segundo) e audio (y Hz)\n"
" -pp <qualidade> habilita filtro de pós processamento (veja detalhes na página\n"
"                 do manual)\n"
" -framedrop      habilita descarte de quadros (para máquinas lentas)\n"
"\n"
"Teclas básicas: (lista completa na páginal do manual, cheque também input.conf)\n"
" <-  ou  ->      retorna/avança 10 segundos\n"
" cima ou baixo   retorna/avança 1 minuto\n"
" pgup ou pgdown  retorna/avança 10 minutos\n"
" < ou >          retorna/avança na lista de reprodução\n"
" p ou ESPAÇO     pausa o filme (pressione qualquer tecla para continuar)\n"
" q ou ESC        para a reprodução e sai do programa\n"
" + ou -          ajusta o atraso do audio de +/- 0.1 segundo\n"
" o               alterna modo OSD: nenhum / busca / busca+cronômetro\n"
" * ou /          aumenta ou diminui o volume pcm\n"
" z ou x          ajusta o atraso da legenda de +/- 0.1 segundo\n"
" r ou t          posição da legenda para cima/baixo, veja também -vf expand\n"
"\n"
"* VEJA A PÁGINA DO MANUAL PARA DETALHES, FUTURAS (AVANÇADAS) OPÇÕES E TECLAS *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nSaindo...\n"
#define MSGTR_ExitingHow "\nSaindo... (%s)\n"
#define MSGTR_Exit_quit "Sair"
#define MSGTR_Exit_eof "Fim do arquivo"
#define MSGTR_Exit_error "Erro fatal"
#define MSGTR_IntBySignal "\nMPlayer interrompido por sinal %d no módulo %s\n"
#define MSGTR_NoHomeDir "Diretório HOME não encontrado.\n"
#define MSGTR_GetpathProblem "Problema em get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Criando arquivo de configuração: %s\n"
#define MSGTR_BuiltinCodecsConf "Usando codecs.conf interno padrão\n"
#define MSGTR_CantLoadFont "Impossível carregar fonte: %s\n"
#define MSGTR_CantLoadSub "Impossível carregar legendas: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: faltando trilha selecionada!\n"
#define MSGTR_CantOpenDumpfile "Impossível abrir arquivo de dump.\n"
#define MSGTR_CoreDumped "core dumped ;)\n"
#define MSGTR_FPSnotspecified "Quadros por segundo não especificado no cabeçalho ou inválido, use a opção -fps.\n"
#define MSGTR_TryForceAudioFmtStr "Tentando forçar a família %s do driver do codec de audio...\n"
#define MSGTR_CantFindAudioCodec "Codec para o format de audio 0x%X não encontrado!\n"
#define MSGTR_TryForceVideoFmtStr "Tentando forçar a família %s do driver do codec de vídeo...\n"
#define MSGTR_CantFindVideoCodec "Impossível encontrar codec que se iguale ao -vo selecionado e ao formato de video 0x%X!\n"
#define MSGTR_VOincompCodec "O dispositivo de saída de vídeo selecionado é incompatível com este codec.\n"
#define MSGTR_CannotInitVO "FATAL: Impossível inicializar o driver de vídeo.\n"
#define MSGTR_CannotInitAO "Impossível abrir/inicializar o dispositivo de audio -> sem som\n"
#define MSGTR_StartPlaying "Iníciando reprodução...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"          ***************************************************\n"\
"          * Seu sistema é muito LENTO para reproduzir isto! *\n"\
"          ***************************************************\n\n"\
"Possíveis razões, problemas, soluções:\n"\
"- Mais comum: driver de _audio_ quebrado/falho\n"\
"  - Tente -ao sdl ou use ALSA 0.5 ou a emulação OSS do ALSA 0.9.\n"\
"  - Experimente com diferente valores para -autosync, 30 é um bom começo.\n"\
"- Saída de vídeo lenta\n"\
"  - Tente um driver diferente -vo (-vo help para listar) ou tente -framedrop!\n"\
"- CPU Lento\n"\
"  - Não tente reproduzir um DVD/DivX grande em um CPU lento! Tente\n"\
"    -hardframedrop.\n"\
"- Arquivo corrompido\n"\
"  - Tente várias combinações de -nobps -ni -forceidx -mc 0.\n"\
"- Mídia lenta (montagens NFS/SMB, DVD, VCD etc...)\n"\
"  - Tente -cache 8192.\n"\
"- Você está usando -cache para reproduzir um arquivo AVI não-entrelaçado?\n"\
"  - Tente -nocache.\n"\
"Leia DOCS/HTML/en/video.html para dicas de ajuste/velocidade.\n"\
"Se nenhum destes ajudar você, leia DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer foi compilado SEM suporte a GUI (interface gráfica com o usuário)!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI (interface gráfica com o usuário) requer X11!\n"
#define MSGTR_Playing "Reproduzindo %s\n"
#define MSGTR_NoSound "Audio: sem som.\n"
#define MSGTR_FPSforced "FPS (quadros por segundo) forçado a ser %5.3f (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Compilado com detecção de CPU em tempo real - AVISO - isto não é ideal! Para obter a melhor performance, recompile MPlayer com --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Compilado para CPU x86 com extenções:"
#define MSGTR_AvailableVideoOutputDrivers "Drivers de saída de vídeo disponíveis:\n"
#define MSGTR_AvailableAudioOutputDrivers "Drivers de saída de audio disponíveis:\n"
#define MSGTR_AvailableAudioCodecs "Codecs de audio disponíveis:\n"
#define MSGTR_AvailableVideoCodecs "Codecs de vídeo disponíveis:\n"
#define MSGTR_AvailableAudioFm "\nFamílias/drivers de codec de audio disponíveis (compilados):\n"
#define MSGTR_AvailableVideoFm "\nFamílias/drivers de codec de vídeo disponíveis (compilados):\n"
#define MSGTR_UsingRTCTiming "Usando regulação de tempo Linux hardware RTC (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: impossível ler propriedades\n"
#define MSGTR_NoStreamFound "Trilha não encontrada\n"
#define MSGTR_ErrorInitializingVODevice "Erro abrindo/inicializando o dispositivo da saída de vídeo (-vo)!\n"
#define MSGTR_ForcedVideoCodec "Codec de vídeo forçado: %s\n"
#define MSGTR_ForcedAudioCodec "Codec de audio forçado: %s\n"
#define MSGTR_Video_NoVideo "Vídeo: sem vídeo\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Impossível inicializar os filtros de vídeo (-vf) ou a saída de vídeo (-vo)!\n"
#define MSGTR_Paused "\n================= PAUSADO =================\r"
#define MSGTR_PlaylistLoadUnable "\nIncapaz de carregar a lista de reprodução %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer falhou por uma 'Instrução Ilegal'.\n"\
"  Pode ser um erro no nosso novo código de detecção de CPU em tempo real...\n"\
"  Por favor leia DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer falhou por uma 'Instrução Ilegal'.\n"\
"  Isso frequentemente acontece quando você o exucuta em um CPU diferente do que\n"\
"  aquele para o qual foi compilado/otimizado.\n  Verifique isso!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer falhou por mau uso do CPU/FPU/RAM.\n"\
"  Recompile o MPlayer com --enable-debug e faça um 'gdb backtrace' e\n"\
"  'disassembly'. Para detalhes, veja DOCS/HTML/en/bugreports_what.html#bugreports_crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer falhou. Isso não deveria acontecer.\n"\
"  Pode ser um erro no código do MPlayer _ou_ em seus drivers _ou_ em sua versão\n"\
"  do gcc. Se você acha que é culpa do MPlayer, por favor leia\n"\
"  DOCS/HTML/en/bugreports.html e siga as instruções. Nós não podemos e não vamos ajudar\n"\
"  a não ser que você proveja esta informação quando reportar um possível erro.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Usando controle de arquivo pass3: %s\n"
#define MSGTR_MissingFilename "\nFaltando nome do arquivo!\n\n"
#define MSGTR_CannotOpenFile_Device "Impossível abrir arquivo/dispositivo\n"
#define MSGTR_CannotOpenDemuxer "Impossível abrir \"demuxer\"\n"
#define MSGTR_NoAudioEncoderSelected "\nNenhum codificador de audio (-oac) selecionado! Selecione um ou use -nosound. Use -oac help para listar!\n"
#define MSGTR_NoVideoEncoderSelected "\nNenhum codificador de vídeo (-ovc) selecionado! Selecione um, use -ovc help para listar!\n"
#define MSGTR_CannotOpenOutputFile "Impossível abrir arquivo de saída '%s'\n"
#define MSGTR_EncoderOpenFailed "Falha ao abrir o codificador\n"
#define MSGTR_ForcingOutputFourcc "Forçando saída fourcc para %x [%.4s]\n"
#define MSGTR_DuplicateFrames "\n%d quadro(s) duplicado(s)!\n"
#define MSGTR_SkipFrame "\npulando frame!!!    \n"
#define MSGTR_ErrorWritingFile "%s: erro gravando arquivo.\n"
#define MSGTR_RecommendedVideoBitrate "Bitrate do vídeo recomendado para CD de %s: %d\n"
#define MSGTR_VideoStreamResult "\nTrilha de vídeo: %8.3f kbit/s  (%d B/s)  tamanho: %"PRIu64" bytes  %5.3f segundos  %d quadros\n"
#define MSGTR_AudioStreamResult "\nTrilha de audio: %8.3f kbit/s  (%d B/s)  tamanho: %"PRIu64" bytes  %5.3f segundos\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     método da taxa de bits variável\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(padrão)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           taxa de bits média\n"\
"\n"\
" cbr           taxa de bits constante\n"\
"               Força também modo de codificação CBR nos modos ABR\n"\
"               pré-selecionados subsequentes.\n"\
"\n"\
" br=<0-1024>   especifica a taxa de bits em kBit (somente CBR e ABR)\n"\
"\n"\
" q=<0-9>       qualidade (0-melhor, 9-pior) (somente para VBR)\n"\
"\n"\
" aq=<0-9>      qualidade do algorítmo (0-melhor/mais lento, 9-pior/mais rápido)\n"\
"\n"\
" ratio=<1-100> taxa de compressão\n"\
"\n"\
" vol=<0-10>    configura ganho da entrada de audio\n"\
"\n"\
" mode=<0-3>    (padrão: auto)\n"\
"                0: estéreo\n"\
"                1: estéreo-junto\n"\
"                2: canal duplo\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: não\n"\
"                1: tudo\n"\
"                2: adaptar\n"\
"\n"\
" fast          aciona codificação rápida nos modos VBR pré-selecionados\n"\
"               subsequentes, qualidade muito baixa e altas taxas de bit.\n"\
"\n"\
" preset=<value> provê os ajustes com a mais alta qualidade.\n"\
"                 medium: codificação VBR, qualidade boa\n"\
"                 (taxa de bits entre 150-180 kbps)\n"\
"                 standard:  codificação VBR, qualidade alta\n"\
"                 (taxa de bits entre 170-210 kbps)\n"\
"                 extreme: codificação VBR, qualidade muito alta\n"\
"                 (taxa de bits entre 200-240 kbps)\n"\
"                 insane:  codificação CBR, ajuste para a mais alta qualidade\n"\
"                 (taxa de bits fixa em 320 kbps)\n"\
"                 <8-320>: codificação ABR com a taxa de bits em kbps dada.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo de CD-ROM '%s' não encontrado!\n"
#define MSGTR_ErrTrackSelect "Erro selecionando trilha do VCD!"
#define MSGTR_ReadSTDIN "Lendo de stdin...\n"
#define MSGTR_UnableOpenURL "Impossível abrir URL: %s\n"
#define MSGTR_ConnToServer "Conecatado ao servidor: %s\n"
#define MSGTR_FileNotFound "Arquivo não encontrado: '%s'\n"

#define MSGTR_SMBInitError "Impossível inicializar biblioteca libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Impossível abrir da \"lan\": '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer não foi compilado com suporte a leitura de SMB\n"

#define MSGTR_CantOpenDVD "Impossível abrir dispositivo de DVD: %s\n"
#define MSGTR_DVDnumTitles "Existem %d títulos neste DVD.\n"
#define MSGTR_DVDinvalidTitle "Número do título do DVD inválido: %d\n"
#define MSGTR_DVDnumChapters "Existem %d capítulos neste título de DVD.\n"
#define MSGTR_DVDinvalidChapter "Número do capítulo do DVD inválido: %d\n"
#define MSGTR_DVDnumAngles "Existem %d anglos neste título de DVD.\n"
#define MSGTR_DVDinvalidAngle "Número do anglo do DVD inválido: %d\n"
#define MSGTR_DVDnoIFO "Impossível abrir o arquivo IFO para o título de DVD %d.\n"
#define MSGTR_DVDnoVOBs "Impossível abrir título VOBS (VTS_%02d_1.VOB).\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "AVISO! Cabeçalho %d da trilha de audio redefinido!\n"
#define MSGTR_VideoStreamRedefined "AVISO! Cabeçalho %d da trilha de vídeo redefinido!\n"
#define MSGTR_TooManyAudioInBuffer "\nMuitos pacotes de audio no buffer: (%d em %d bytes).\n"
#define MSGTR_TooManyVideoInBuffer "\nMuitos pacotes de audio no buffer: (%d em %d bytes).\n"
#define MSGTR_MaybeNI "Talvez você esteja reproduzindo um fluxo/arquivo não-entrelaçado ou o codec falhou?\n" \
		      "Para arquivos .AVI, tente forçar um modo não-entrelaçado com a opção -ni.\n"
#define MSGTR_SwitchToNi "\nDetectado .AVI mau entrelaçado - mudando para o modo -ni!\n"
#define MSGTR_Detected_XXX_FileFormat "Detectado formato de arquivo %s!\n"
#define MSGTR_DetectedAudiofile "Detectado arquivo de audio!\n"
#define MSGTR_NotSystemStream "Formato do fluxo não MPEG System... (pode ser um fluxo de transporte?)\n"
#define MSGTR_InvalidMPEGES "Fluxo MPEG-ES inválido??? Contacte o autor, pode ser um bug :(\n"
#define MSGTR_FormatNotRecognized "======= Desculpe, este formato de arquivo não é reconhecido/suportado ========\n"\
				  "== Se este arquivo é um fluxo AVI, ASF ou MPEG, por favor contacte o autor ==\n"
#define MSGTR_MissingVideoStream "Nenhuma trilha de vídeo encontrado!\n"
#define MSGTR_MissingAudioStream "Nenhuma trilha de audio encontrado -> sem som\n"
#define MSGTR_MissingVideoStreamBug "Trilha de vídeo faltando!? Contacte o autor, pode ser um bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Arquivo não contém a trilha de audio ou vídeo selecionado.\n"

#define MSGTR_NI_Forced "Forçado"
#define MSGTR_NI_Detected "Detectado"
#define MSGTR_NI_Message "%s formato de arquivo AVI NÃO ENTRELAÇADO!\n"

#define MSGTR_UsingNINI "Usando formato quebrado não-entrelaçado do arquivo AVI!\n"
#define MSGTR_CouldntDetFNo "Impossível determinar o número de quadros (para busca absoluta)  \n"
#define MSGTR_CantSeekRawAVI "Impossível buscar em fluxos de .AVI brutos! (índice requerido, tente com a opção -idx!)  \n"
#define MSGTR_CantSeekFile "Impossível buscar neste arquivo!  \n"

#define MSGTR_MOVcomprhdr "MOV: Cabeçalhos comprimidos não suportados (ainda)!\n"
#define MSGTR_MOVvariableFourCC "MOV: Advertência! Variável FOURCC detectada!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Advertência! Trilhas demais!"
#define MSGTR_FoundAudioStream "==> Trilha de audio encontrada: %d\n"
#define MSGTR_FoundVideoStream "==> Trilha de video encontrada: %d\n"
#define MSGTR_DetectedTV "TV detectada! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Impossível abrir o demuxer ogg\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Procurando por trilha de audio (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Impossível abrir trilha de audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Impossível abrir trilha de legendas: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Falha ao abrir demuxer de audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Falha ao abrir demuxer de legendas: %s\n"
#define MSGTR_TVInputNotSeekable "Entrada de TV não aceita busca! (Provavelmente a busca será pra mudar de canal ;)\n"
#define MSGTR_ClipInfo "Informações do clip:\n"


// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Impossível abrir codec\n"
#define MSGTR_CantCloseCodec "Impossível fechar codec\n"

#define MSGTR_MissingDLLcodec "ERRO: Impossível abrir o codec DirectShow %s requerido.\n"
#define MSGTR_ACMiniterror "Impossível carregar/inicializar o codec Win32/ACM AUDIO (arquivo DLL faltando?).\n"
#define MSGTR_MissingLAVCcodec "Impossível encontrar codec '%s' em libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF enquanto procurando pelo cabeçalho da sequência\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Impossível ler cabeçalho da sequência!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Impossível ler extenção do cabeçalho da sequência!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Cabeçalho da sequência mau!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Extensão do cabeçalho da sequência mau!\n"

#define MSGTR_ShMemAllocFail "Impossível alocar memória compartilhada\n"
#define MSGTR_CantAllocAudioBuf "Impossível alocate buffer da saída de audio\n"

#define MSGTR_UnknownAudio "Desconhecido/faltando formato de audio -> sem som\n"

#define MSGTR_UsingExternalPP "[PP] Usando filtro de pós processamento externo, máximo q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Usando pós processamento do codec, máximo q = = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atributo de vídeo '%s' não é suportado pelo vo & vd selecionado! \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Família [%s] (vfm=%s) do codec de video não disponível (habilite na hora da compilação!)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Família [%s] (afm=%s) do codec de audio não disponível (habilite na hora da compilação!)\n"
#define MSGTR_OpeningVideoDecoder "Abrindo decodificador de vídeo: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Abrindo decodificador de audio: [%s] %s\n"
#define MSGTR_UninitVideoStr "finalizando vídeo: %s\n"
#define MSGTR_UninitAudioStr "finalizando audio: %s\n"
#define MSGTR_VDecoderInitFailed "Falha na incialização do VDecoder :(\n"
#define MSGTR_ADecoderInitFailed "Falha na incialização do ADecoder :(\n"
#define MSGTR_ADecoderPreinitFailed "Falha na pré-inicialização do ADecoder :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Aclocando %d bytes para o buffer de entrtada\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alocando %d + %d = %d bytes para o buffer de saída\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurando o suporte a lirc...\n"
#define MSGTR_LIRCopenfailed "Falha na abertura do suporte a lirc!\n"
#define MSGTR_LIRCcfgerr "Falha ao ler o arquivo de configuração do LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Filtro de vídeo '%s' não encontrado\n"
#define MSGTR_CouldNotOpenVideoFilter "Impossível abrir o filtro de vídeo '%s'\n"
#define MSGTR_OpeningVideoFilter "Abrindo filtro de vídeo: "
#define MSGTR_CannotFindColorspace "Impossível encontrar um \"colorspace\" comum, mesmo inserindo \"scale\" :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: codec não configurou sh->disp_w e sh->disp_h, tentando solução alternativa!\n"
#define MSGTR_VoConfigRequest "VDec: configuração vo pedida - %d x %d (preferido csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Impossível encotrar \"colorspace\" similar - retentando com -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Aspecto do filme é  %.2f:1 - pré-redimensionando para corrigir o aspecto do filme.\n"
#define MSGTR_MovieAspectUndefined "Aspecto do filme é indefinido - nenhum pré-redimensionamento aplicado.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Sobre"
#define MSGTR_FileSelect "Selecionar arquivo..."
#define MSGTR_SubtitleSelect "Selecionar legenda..."
#define MSGTR_OtherSelect "Selecionar..."
#define MSGTR_AudioFileSelect "Selecionar canal de audio externo..."
#define MSGTR_FontSelect "Selecionar fonte..."
#define MSGTR_PlayList "Lista de reprodução"
#define MSGTR_Equalizer "Equalizador"
#define MSGTR_SkinBrowser "Skins"
#define MSGTR_Network "Rede..."
#define MSGTR_Preferences "Preferências"
#define MSGTR_NoMediaOpened "Nenhuma mídia aberta."
#define MSGTR_VCDTrack "Trilha do VCD %d"
#define MSGTR_NoChapter "Nenhum capítulo"
#define MSGTR_Chapter "Capítulo %d"
#define MSGTR_NoFileLoaded "Nenhum arquivo carregado"

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Cancelar"
#define MSGTR_Add "Adicionar"
#define MSGTR_Remove "Remover"
#define MSGTR_Clear "Limpar"
#define MSGTR_Config "Configurações"
#define MSGTR_ConfigDriver "Configurações do driver"
#define MSGTR_Browse "Procurar"

// --- error messages ---
#define MSGTR_NEMDB "Desculpe, sem memória suficiente para desenhar o buffer."
#define MSGTR_NEMFMR "Desculpe, sem memória suficiente para rendenizar o menu."
#define MSGTR_IDFGCVD "Desculpe, eu não encontrei um driver saída de vídeo compatível com GUI."
#define MSGTR_NEEDLAVC "Desculpe, você não pode reproduzir arquivos não-MPEG com o seu dispositivo DXR3/H+ sem recodificar.\nPor favor habilite lavc na configuração do DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] erro no arquivo de configuração do skin na linha %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] aviso no arquivo de configuração do skin na linha %d: widget encontrado mas antes de \"section\" não encontrado (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] aviso no arquivo de configuração do skin na linha %d: widget encontrado mas antes de \"subsection\" não encontrtado (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] aviso no arquivo de configuração do skin na linha %d: esta sub-seção não é suportada por este widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bit ou menor profundidade de cores não suportado (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "arquivo não encontrado (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "erro na leitura do BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "erro na leitura do TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "erro na leitura do PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "TGA empacotado RLE não suportado (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "tipo de arquivo desconhecido (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "erro na conversão 24 bit para 32 bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "mensagem desconhecida: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "memória insuficiente\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "muitas fontes declaradas\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "arquivo da fonte não encontrado\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "arquivo de imagem da fonte não encontrado\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "indentificador de fonte não existente (%s)\n"
#define MSGTR_SKIN_UnknownParameter "parâmetro desconhecido (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin não encontrado (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Erro na leitura do arquivo de configuração do skin (%s).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Sobre o MPlayer"
#define MSGTR_MENU_Open "Abrir..."
#define MSGTR_MENU_PlayFile "Reproduzir arquivo..."
#define MSGTR_MENU_PlayVCD "Reproduzir VCD..."
#define MSGTR_MENU_PlayDVD "Reproduzir DVD..."
#define MSGTR_MENU_PlayURL "Reproduzir URL..."
#define MSGTR_MENU_LoadSubtitle "Carregar legenda..."
#define MSGTR_MENU_DropSubtitle "Descartar legenda..."
#define MSGTR_MENU_LoadExternAudioFile "Carregar arquivo de audio externo..."
#define MSGTR_MENU_Playing "Reprodução"
#define MSGTR_MENU_Play "Reproduzir"
#define MSGTR_MENU_Pause "Pausar"
#define MSGTR_MENU_Stop "Parar"
#define MSGTR_MENU_NextStream "Próxima faixa"
#define MSGTR_MENU_PrevStream "Faixa anterior"
#define MSGTR_MENU_Size "Tamanho"
#define MSGTR_MENU_NormalSize "Tamanho normal"
#define MSGTR_MENU_DoubleSize "Tamanho dobrado"
#define MSGTR_MENU_FullScreen "Tela cheia"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Abrir disco..."
#define MSGTR_MENU_ShowDVDMenu "Mostrar menu do DVD"
#define MSGTR_MENU_Titles "Títulos"
#define MSGTR_MENU_Title "Título %2d"
#define MSGTR_MENU_None "(nenhum)"
#define MSGTR_MENU_Chapters "Capítulos"
#define MSGTR_MENU_Chapter "Capítulo %2d"
#define MSGTR_MENU_AudioLanguages "Idiomas do audio"
#define MSGTR_MENU_SubtitleLanguages "Idiomas da legenda"
#define MSGTR_MENU_SkinBrowser "Skins"
#define MSGTR_MENU_Exit "Sair..."
#define MSGTR_MENU_Mute "Mudo"
#define MSGTR_MENU_Original "Original"
#define MSGTR_MENU_AspectRatio "Aspecto"
#define MSGTR_MENU_AudioTrack "Trilha de audio"
#define MSGTR_MENU_Track "Trilha %d"
#define MSGTR_MENU_VideoTrack "Trilha de vídeo"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Vídeo"
#define MSGTR_EQU_Contrast "Contraste: "
#define MSGTR_EQU_Brightness "Brilho: "
#define MSGTR_EQU_Hue "Cor: "
#define MSGTR_EQU_Saturation "Saturação: "
#define MSGTR_EQU_Front_Left "Frente Esquerda"
#define MSGTR_EQU_Front_Right "Frente Direita"
#define MSGTR_EQU_Back_Left "Fundo Esquerda"
#define MSGTR_EQU_Back_Right "Fundo Direita"
#define MSGTR_EQU_Center "Centro"
#define MSGTR_EQU_Bass "Grave"
#define MSGTR_EQU_All "Todos"
#define MSGTR_EQU_Channel1 "Canal 1:"
#define MSGTR_EQU_Channel2 "Canal 2:"
#define MSGTR_EQU_Channel3 "Canal 3:"
#define MSGTR_EQU_Channel4 "Canal 4:"
#define MSGTR_EQU_Channel5 "Canal 5:"
#define MSGTR_EQU_Channel6 "Canal 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Caminho"
#define MSGTR_PLAYLIST_Selected "Arquivos selecionados"
#define MSGTR_PLAYLIST_Files "Arquivos"
#define MSGTR_PLAYLIST_DirectoryTree "Árvore de diretórios"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Legenda & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs & demuxer"
#define MSGTR_PREFERENCES_Misc "Misc"

#define MSGTR_PREFERENCES_None "Nenhum"
#define MSGTR_PREFERENCES_AvailableDrivers "Drivers disponíveis:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Não reproduzir som"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizar som"
#define MSGTR_PREFERENCES_EnableEqualizer "Habilitar equalizador"
#define MSGTR_PREFERENCES_ExtraStereo "Habilitar extra estéreo"
#define MSGTR_PREFERENCES_Coefficient "Coeficiente:"
#define MSGTR_PREFERENCES_AudioDelay "Atraso do audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Habilitar duplo buffer"
#define MSGTR_PREFERENCES_DirectRender "Habilitar direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Habilitar descarte de quadros"
#define MSGTR_PREFERENCES_HFrameDrop "Habilitar descarte de quadros SEVERO (perigoso)"
#define MSGTR_PREFERENCES_Flip "Inverter imagem verticalmente"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Temporizador e indicadores"
#define MSGTR_PREFERENCES_OSDProgress "Barras de progresso apenas"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Temporizador, porcentagem e tempo total"
#define MSGTR_PREFERENCES_Subtitle "Legenda:"
#define MSGTR_PREFERENCES_SUB_Delay "Atrtaso: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Posição: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Desabilitar auto carregamento de legendas"
#define MSGTR_PREFERENCES_SUB_Unicode "Legenda unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Converter a legenda dada para o formato de legenda do MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Converter a legenda dada para o formato baseado em tempo SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Sobreposição da legenda"
#define MSGTR_PREFERENCES_Font "Fonte"
#define MSGTR_PREFERENCES_FontFactor "Fator da fonte:"
#define MSGTR_PREFERENCES_PostProcess "Habilitar pós-processamento"
#define MSGTR_PREFERENCES_AutoQuality "Qualidade do audio: "
#define MSGTR_PREFERENCES_NI "Usar leitor de AVI não-entrelaçado"
#define MSGTR_PREFERENCES_IDX "Reconstruir tabela índice, se necessário"
#define MSGTR_PREFERENCES_VideoCodecFamily "Família do codec de vídeo:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Família do codec de audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Nível do OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Legenda"
#define MSGTR_PREFERENCES_FRAME_Font "Fonte"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Pós-processamento"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_Message "Por favor lembre que você precisa reiniciar a reprodução para algumas opções fazerem efeito!"
#define MSGTR_PREFERENCES_DXR3_VENC "Codificador de video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Usar LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Línguas Européias Ocidentais (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Línguas Européias Ocidentais com Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Linguas Européias Eslávicas/Centrais (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galego, Maltês, Turco (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Caracteres Bálticos Antigos (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cirílico (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Árabe (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Grego Moderno (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turco (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Báltico (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celta (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Caracteres Hebraicos (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Russo (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ucraniano, Bielo-Russo (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Caracteres Chineses Simplificados (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Caracteres Chineses Tradicionais (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Caracteres Japoneses (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Caracteres Coreanos (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Caracteres Tailandeses (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Windows Cirílico (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Européias Eslávicas/Centrais Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Sem auto redimensionamento"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcional a largura do filme"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcional a altura do filme"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcional a diagonal do filme"
#define MSGTR_PREFERENCES_FontEncoding "Codificação:"
#define MSGTR_PREFERENCES_FontBlur "Embassar:"
#define MSGTR_PREFERENCES_FontOutLine "Contorno:"
#define MSGTR_PREFERENCES_FontTextScale "Tamanho do texto:"
#define MSGTR_PREFERENCES_FontOSDScale "Tamanho do OSD:"
#define MSGTR_PREFERENCES_Cache "Cache ligado/desligado"
#define MSGTR_PREFERENCES_CacheSize "Tamaho do cache: "
#define MSGTR_PREFERENCES_LoadFullscreen "Iniciar em tela cheia"
#define MSGTR_PREFERENCES_XSCREENSAVER "Parar XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Habilitar barra de reprodução"
#define MSGTR_PREFERENCES_AutoSync "AutoSync ligado/desligado"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "Dispositivo de CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Dispositivo de DVD:"
#define MSGTR_PREFERENCES_FPS "Quadros por segundo do filme:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Mostrar janela do vídeo quando inativo"
#define MSGTR_ABOUT_UHU "Desenvolvimento do GUI patrocinado por UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Erro fatal!"
#define MSGTR_MSGBOX_LABEL_Error "Erro!"
#define MSGTR_MSGBOX_LABEL_Warning "Atenção!"

#endif
