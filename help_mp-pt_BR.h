// Translated by Fabio Pugliese Ornellas <fabio.ornellas@poli.usp.br>
// Portuguese from Brazil Translation
// GPLed code

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (veja DOCS!)\n"
"\n";

static char help_text[]=
"Uso:   mplayer [opções] [caminho/]nomedoarquivo\n"
"\n"
"Opções:\n"
" -vo <drv[:dev]> seleciona driver de saída de video & dispositivo (veja '-vo help' para obter uma lista)\n"
" -ao <drv[:dev]> seleciona driver de saída de audio & dispositivo (veja '-ao help' para obter uma lista)\n"
" -vcd <trackno>  reproduz faixa de VCD (video cd) do dispositivo ao invés de arquivo regular\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  especifica dispositivo de DVD para autenticação (para discos encriptados)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  reproduz título/faixa do dispositivo de DVD ao inves de arquivo regular\n"
#endif
" -ss <timepos>   busca uma determinada posição (segundos ou hh:mm:ss)\n"
" -nosound        não reproduz som\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  seleciona a saída estéreo MPEG1 (0:estéreo 1:esquerda 2:direita)\n"
#endif
" -channels <n>   número de canais de saída de audio\n"
" -fs -vm -zoom   opções de reprodução em tela cheia (tela cheia,muda modo de vídeo,redimensionamento por software)\n"
" -x <x> -y <y>   redimensiona a imagem para a resolução <x> * <y> [se o dispositivo -vo suporta!]\n"
" -sub <file>     especifica o arquivo de legenda a usar (veja também -subfps, -subdelay)\n"
" -playlist <file> especifica o aruqivo com a lista de reprodução\n"
" -vid x -aid y   opções para selecionar o fluxo (stream) de vídeo (x) e audio (y) a reproduzir\n"
" -fps x -srate y opções para mudar quadros por segundo (fps) do vídeo (x) e frequência (em Hz) do audio\n"
" -pp <quality>   habilita filtro de pós-processamento (0-4 para DivX, 0-63 para mpegs)\n"
" -nobps          usa um método alternativo de sincronia audio/vídeo para arquivos AVI (pode ajudar!)\n"
" -framedrop      habilita descarte de frames (para maquinas lentas)\n"
" -wid <window id> usa a janela existente para a saída de vídeo (útil com plugger!)\n"
"\n"
"Teclas:\n"
" <-  ou  ->      avança/retorna 10 segundos\n"
" cima ou baixo   avança/retorna 1 minuto\n"
" < ou >          avança/retorna na lista de reprodução\n"
" p ou ESPAÇO     paraliza o filme (pressione qualqer tecla para continuar)\n"
" q ou ESC        para de reproduzir e sai do programa\n"
" + ou -          ajusta o atraso do audio de +/- 0.1 segundo\n"
" o               muda o modo OSD: nenhum / busca / busca+tempo\n"
" * ou /          incrementa ou decrementa o volume (pressione 'm' para selecionar entre master/pcm)\n"
" z ou x          ajusta o atraso da legenda de +/- 0.1 segundo\n"
"\n"
" * * * VEJA A PAGINA DO MANUAL PARA DETALHES, FUTURAS OPÇÕES (AVANÇADAS) E TECLAS ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nSaindo... (%s)\n"
#define MSGTR_Exit_frames "Número de frames requisitados reprodizidos"
#define MSGTR_Exit_quit "Sair"
#define MSGTR_Exit_eof "Fim da linha"
#define MSGTR_Exit_error "Erro fatal"
#define MSGTR_IntBySignal "\nMPlayer interrompido com sinal %d no módulo: %s \n"
#define MSGTR_NoHomeDir "Diretório HOME não encontrado\n"
#define MSGTR_GetpathProblem "Problema em get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Criando arquivo de configuração: %s\n"
#define MSGTR_InvalidVOdriver "Dispositivo de saída de vídeo inválido: %s\nUse '-vo help' para obter uma lista dos dispositivos de vídeo disponíveis.\n"
#define MSGTR_InvalidAOdriver "Dispositivo de saída de áudio inválido: %s\nUse '-ao help' para obter uma lista dos dispositivos de áudio disponíveis.\n"
#define MSGTR_CopyCodecsConf "(copie/ln etc/codecs.conf (da árvore fonte do MPlayer) para ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Não pode-se carregar a fonte: %s\n"
#define MSGTR_CantLoadSub "Não pode-se carregar a legenda: %s\n"
#define MSGTR_ErrorDVDkey "Erro processado a cahve do DVD.\n"
#define MSGTR_CmdlineDVDkey "Linha de comando requisitada do DVD está armazenada para \"descrambling\".\n"
#define MSGTR_DVDauthOk "Sequência de autenticação do DVD parece estar OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: fluxo (stream) selecionado faltando!\n"
#define MSGTR_CantOpenDumpfile "Nào pode-se abrir o arquivo dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "Quadros por segundo (FPS) não especificado (ou inválido) no cabeçalho! User a opção -fps!\n"
#define MSGTR_NoVideoStream "Desculpe, sem fluxo (stream) de vídeo... ainda não é reproduzível\n"
#define MSGTR_TryForceAudioFmt "Tentando forçar a família do codec do dispositivo de áudio %d ...\n"
#define MSGTR_CantFindAfmtFallback "Impossível encontrar codec de áudio para a família de dispositívo forçada, voltando a outros dispositívos.\n"
#define MSGTR_CantFindAudioCodec "Impossível encontrar codec para o formato de áudio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Tente atualizar $s de etc/codecs.conf\n*** Se ainda não estiver OK, então leia DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Impossível inicializar o codec de áudio! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Tentando forçar família do codec do dispositivo de vídeo %d ...\n"
#define MSGTR_CantFindVfmtFallback "Impossível encontrar codec de vídeo para a família de dispositivo forçada, voltando a outros dispositivos.\n"
#define MSGTR_CantFindVideoCodec "Impossível encontrar codec que bata com o selecionado -vo e o formato de vídeo 0x%X !\n"
#define MSGTR_VOincompCodec "Desculpe, o dispositivo de saída de vídeo video_out é incompatível com este codec.\n"
#define MSGTR_EncodeFileExists "Arquivo já exixte: %s (não sobreescreva sui AVI favorito!)\n"
#define MSGTR_CantCreateEncodeFile "Impossível criar arquivo para codificação\n"
#define MSGTR_CannotInitVO "FATAL: Impossível inicializar o dispositivo de vídeo!\n"
#define MSGTR_CannotInitAO "Impossível abrir/inicializar o dispositívo de áudio -> NOSOUND\n"
#define MSGTR_StartPlaying "Início da reprodução...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         * Seu sistema é muito LENTO para reproduzir isto!*\n"\
"         ************************************************\n"\
"!!! Possíveis causas, problemas, soluções:  \n"\
"- Mais comum: dispositivo de áudio quebrado/buggy. Solução: tente -ao sdl\n"\
"  ou use ALSA 0.5 ou emulação do OSS para ALSA 0.9. Leia DOCS/sound.html\n"\
"  para mais dicas!\n"\
"- Saída de vídeo lenta. Tende um dispositivo diferente com -vo (para lista:\n"\
"  -vo help) ou tente com -framedrop ! Leia DOCS/video.html para mais dicas\n"\
"  de como aumentar a velocidade do vídeo.\n"\
"- CPU lento. Não tente reproduzir grandes DVD/DivX em CPU lento! Tente\n"\
"  -hardframedrop\n"\
"- Arquivo corrompido. Tente várias combinaçoes destes: -nobps  -ni  -mc 0\n"\
"  -forceidx. Se nenhum destes resolver, leia DOCS/bugreports.html !\n"\

#define MSGTR_NoGui "MPlayer foi compilado sem suporte a GUI (interface gráfica)!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI requer X11!\n"
#define MSGTR_Playing "Reproduzindo %s\n"
#define MSGTR_NoSound "Áudio: nosound!!!\n"
#define MSGTR_FPSforced "FPS forçado a ser %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo de CD-ROM '%s' não encontrado!\n"
#define MSGTR_ErrTrackSelect "Erro selecionando a faixa do VCD!"
#define MSGTR_ReadSTDIN "Lendo de stdin...\n"
#define MSGTR_UnableOpenURL "Impossível abrir URL: %s\n"
#define MSGTR_ConnToServer "Conectado ao servidor: %s\n"
#define MSGTR_FileNotFound "Arquivo não encontrado: '%s'\n"

#define MSGTR_CantOpenDVD "Impossível abrir o dispositivo de DVD: %s\n"
#define MSGTR_DVDwait "Lendo estrutura do disco, por favor espere...\n"
#define MSGTR_DVDnumTitles "Existem %d títulos neste DVD.\n"
#define MSGTR_DVDinvalidTitle "Número do título do DVD inválido: %d\n"
#define MSGTR_DVDnumChapters "Existem %d capítulos neste títulod de DVD.\n"
#define MSGTR_DVDinvalidChapter "Número do capítulo do DVD inválido: %d\n"
#define MSGTR_DVDnumAngles "Existem %d angulos neste títulod e DVD.\n"
#define MSGTR_DVDinvalidAngle "Número do angulo do DVD inválido: %d\n"
#define MSGTR_DVDnoIFO "Impossível abrir arquivo IFO para o título %d do DVD.\n"
#define MSGTR_DVDnoVOBs "Impossível abrir os títulos VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD aberto com sucesso!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Aviso! Cabeçalho do fluxo (stream) de áudio %d redefinido!\n"
#define MSGTR_VideoStreamRedefined "Aviso! Cabeçalho do fluxo (strean) de vídeo %d redefinido!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Muitos (%d em %d bytes) pacotes de áudio no buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Muitos (%d em %d bytes) pacotes de vídeo no buffer!\n"
#define MSGTR_MaybeNI "(pode ser que você reprodiziu um não-\"interleaved\" fluxo(stream)/arquivo ou o coded falhou)\n"
#define MSGTR_DetectedFILMfile "Detectado formato de arquivo FILM!\n"
#define MSGTR_DetectedFLIfile "Detectado formato de arquivo FLI!\n"
#define MSGTR_DetectedROQfile "Detectado formato de arquivo RoQ!\n"
#define MSGTR_DetectedREALfile "Detectado formato de arquivo REAL!\n"
#define MSGTR_DetectedAVIfile "Detectado formato de arquivo AVI!\n"
#define MSGTR_DetectedASFfile "Detectado formato de arquivo ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Detectado formato de arquivo MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Detectado formato de arquivo MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Detectado formato de arquivo MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Detectado formato de arquivo QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Fluxo (stream) de vídeo MPEG faltando!? Contate o autor, pode ser um bug :(\n"
#define MSGTR_InvalidMPEGES "Fluxo (stream) de vídeo MPEG-ES faltando!? Contate o autor, pode ser um bug :(\n"
#define MSGTR_FormatNotRecognized "============= Desculpe, este formato não é rconhecido/suportado ===============\n"\
				  "=== Se este arquivo é um AVI, ASF ou MPEG, por favor contate o autor! ===\n"
#define MSGTR_MissingVideoStream "Fluxo (stream) de vídeo não encontrado!\n"



#define MSGTR_MissingAudioStream "Fluxo (stream) de áudio não encontrado...  -> nosound\n"
#define MSGTR_MissingVideoStreamBug "Fluxo (stream) de vídeo faltando!? Contate o autor, pode ser um bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: arquivo não contém o fluxo (stream) de áudio ou vídeo selecionado\n"

#define MSGTR_NI_Forced "Forçado"
#define MSGTR_NI_Detected "Detectado"
#define MSGTR_NI_Message "Formato do arquivo AVI %s NÃO-\"INTERLEAVED\"!\n"

#define MSGTR_UsingNINI "Usando formato de arquivo AVI NÃO-\"INTERLEAVED\" quebrado!\n"
#define MSGTR_CouldntDetFNo "Impossível determinar o número de frames (para busca absoluta)  \n"
#define MSGTR_CantSeekRawAVI "Impossível buscar em fluxos (streams) de .AVI raw! (índice requerido, tente com a opção -idx ativada!)  \n"
#define MSGTR_CantSeekFile "Impossível buscar neste arquivo!  \n"

#define MSGTR_EncryptedVOB "Arquivo VOB encriptado (não compilado com suporte a libcss!) Leia o arquivo DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Fluxo (stream) encriptado mas a autenticação nao foi requisitada por você!!\n"

#define MSGTR_MOVcomprhdr "MOV: Cabeçalhos comprimidos (ainda) não suportados!\n"
#define MSGTR_MOVvariableFourCC "MOV: Aviso! variável FOURCC detectada!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Aviso! muitas trilhas!"
#define MSGTR_MOVnotyetsupp "\n****** Formato Quicktime MOV ainda não suportado!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "impossível abrir codec\n"
#define MSGTR_CantCloseCodec "impossível fechar codec\n"

#define MSGTR_MissingDLLcodec "ERRO: Impossível abrir o codec DirectShow requerido: %s\n"
#define MSGTR_ACMiniterror "Impossível carregar/inicializar o codec Win32/ACM AUDIO (faltando o arquivo DLL?)\n"
#define MSGTR_MissingLAVCcodec "Impossível encontrar codec '%s' em libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer foi compilado SEM suporte a DirectShow!\n"
#define MSGTR_NoWfvSupport "Suporte aos codecs win32 deshabilitado, ou indisponível em  plataformas não-x86!\n"
#define MSGTR_NoDivx4Support "MPlayer foi compilado SEM suporte a DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer foi compilado SEM suporte a ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Codec de áudio Win32/ACM deshabilitado, ou indisponível em CPU não-x86 -> force nosound :(\n"
#define MSGTR_NoDShowAudio "Compilado sem suporte a DirectShow -> force nosound :(\n"
#define MSGTR_NoOggVorbis "Codec de áudio OggVorbis deshabilitado -> force nosound :(\n"
#define MSGTR_NoXAnimSupport "MPlayer foi compilado SEM suporte a XAnim!\n"

#define MSGTR_MpegPPhint "AVISO! Você requisitou um pós-processamento de imagem para um\n" \
			 "         vídeo MPEG 1/2, mas compilou o MPlayer sem suporte a pós-processametno\n" \
			 "         para MPEG 1/2!\n" \
			 "         #define MPEG12_POSTPROC em config.h, e recompile a libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF enquanto procurava pela sequência de cabeçalho\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Impossível ler a sequência do cabeçalho!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Impossível ler a extensão da sequência de cabeçalhon"
#define MSGTR_BadMpegSequHdr "MPEG: Sequência de cabeçalho ruim!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Extenção da sequência de cabeçalho ruim!\n"

#define MSGTR_ShMemAllocFail "Impossível alocar memória compartilahda\n"
#define MSGTR_CantAllocAudioBuf "Impossível alocar a saída de áudio no buffer\n"
#define MSGTR_NoMemForDecodedImage "Sem memória suficiente para alocar o buffer de imagem (%ld bytes)\n"

#define MSGTR_AC3notvalid "Fluxo (stream) AC3 inválido.\n"
#define MSGTR_AC3only48k "Somente fluxos (streams) de 48000 Hz são suportadas.\n"
#define MSGTR_UnknownAudio "Formato de áudio desconhecido/faltando, usando nosound\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurando suporte a lirc...\n"
#define MSGTR_LIRCdisabled "Você não poderá usar seu controle remoto\n"
#define MSGTR_LIRCopenfailed "Falha abrindo o suporte a lirc\n"
#define MSGTR_LIRCsocketerr "Algo está errado com o socket lirc: %s\n"
#define MSGTR_LIRCcfgerr "Falha ao ler o arquivo de configuração do LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Sobre"
#define MSGTR_FileSelect "Selecionar arquivo ..."
#define MSGTR_SubtitleSelect "Selecionar legenda ..."
#define MSGTR_OtherSelect "Selecionar ..."
#define MSGTR_MessageBox "Caixa de Mensagem"
#define MSGTR_PlayList "Lista de Reprocução"
#define MSGTR_SkinBrowser "Visualizador de texturas"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Cancelar"
#define MSGTR_Add "Add"
#define MSGTR_Remove "Remover"

// --- error messages ---
#define MSGTR_NEMDB "Desculpe, sem memória suficiente para desenhar o buffer."
#define MSGTR_NEMFMR "Desculpe, sem memória suficiente para rendenizar o menu."
#define MSGTR_NEMFMM "Desculpe, sem memória suficiente para a mascara da forma da janela principal."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] erro no arquivo de configuração da textura na linha %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] aviso no arquivo de configuração da textura na linha %d: widget encontrado mas antes de \"section\" não encontrado ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] aviso no arquivo de configuração da textura na linha %d: widget encontrado mas antes de \"subsection\" não encontrado (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits ou menos cores não suportado ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "arquivo não encontrado ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "erro na leitura do bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "erro na leitura do tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "erro na leitura do png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "Pacote RLE no tga não suportado ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "tipo de arquivo desconhecido ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "erro na conversão de 24 bit para 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "menssagem desconhecida: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "sem memoria suficiente\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "fontes de mais declaradas\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "arquivo da fonte não encontrado\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "arquivo da imagem da fonte não encontrado\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificador de fonte inexistente ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "parâmetro desconhecido ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] sem memóra suficiente.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Textura não encontrada ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Erro na leitura do arquivo de configuração da textura ( %s ).\n"
#define MSGTR_SKIN_LABEL "Texturas:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Sobre MPlayer"
#define MSGTR_MENU_Open "Abrir ..."
#define MSGTR_MENU_PlayFile "Reproduzir arquivo ..."
#define MSGTR_MENU_PlayVCD "Reproduzir VCD ..."
#define MSGTR_MENU_PlayDVD "Reproduzir DVD ..."
#define MSGTR_MENU_PlayURL "Reproduzir URL ..."
#define MSGTR_MENU_LoadSubtitle "Carregar legenda ..."
#define MSGTR_MENU_Playing "Reproduzindo"
#define MSGTR_MENU_Play "Reproduzir"
#define MSGTR_MENU_Pause "Paralizar"
#define MSGTR_MENU_Stop "Parar"
#define MSGTR_MENU_NextStream "Proximo arquivo"
#define MSGTR_MENU_PrevStream "Arquivo anterior"
#define MSGTR_MENU_Size "Tamanho"
#define MSGTR_MENU_NormalSize "Tamanho normal"
#define MSGTR_MENU_DoubleSize "Tamanho dobrado"
#define MSGTR_MENU_FullScreen "Tela cheia"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Reproduzir disco ..."
#define MSGTR_MENU_ShowDVDMenu "Mostrar menu do DVD"
#define MSGTR_MENU_Titles "Títulos"
#define MSGTR_MENU_Title "Título %2d"
#define MSGTR_MENU_None "(vazio)"
#define MSGTR_MENU_Chapters "Capítulos"
#define MSGTR_MENU_Chapter "Capítulo %2d"
#define MSGTR_MENU_AudioLanguages "Idiomas do áudio"
#define MSGTR_MENU_SubtitleLanguages "Idioma das legendas"
#define MSGTR_MENU_PlayList "Lista de reprodução"
#define MSGTR_MENU_SkinBrowser "Visualizador de texturas"
#define MSGTR_MENU_Preferences "Preferências"
#define MSGTR_MENU_Exit "Sair ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "erro fatal ..."
#define MSGTR_MSGBOX_LABEL_Error "erro ..."
#define MSGTR_MSGBOX_LABEL_Warning "aviso ..."

#endif
