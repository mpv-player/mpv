![mpv logo](https://raw.githubusercontent.com/mpv-player/mpv.io/master/source/images/mpv-logo-128.png)

# mpv


* [Links externos](#links-externos)
* [Resumo](#resumo)
* [Requerimentos de sistema](#requerimentos-de-sistema)
* [Downloads](#downloads)
* [Registo de alterações](#registo-de-alterações)
* [Compilação](#compilação)
* [Ciclo de atualização](#ciclo-de-atualização)
* [Reportagem de bugs](#reportagem-de-bugs)
* [Contribuir](#contribuir)
* [Licença](#licença)
* [Contato](#contato)


## Links externos


* [Wiki](https://github.com/mpv-player/mpv/wiki)
* [Perguntas recentes][perguntas-frequentes]
* [Manual](https://mpv.io/manual/master/)


## Resumo


**mpv** é um reprodutor de mídia gratuito e livre (em termos de liberdade) para a linha de comando. Ele suporta
uma grande variedade de formatos de arquivos de mídia, codecs de áudio e video, e tipos de legenda.

Temos uma seção de [perguntas recentes][perguntas-frequentes].

Atualizações podem ser encontrados na [lista de atualizações][atualizações].

## Requerimentos de sistema

- Um Linux que não é muito antigo, Windows 7 ou posterior, ou OSX 10.8 ou posterior.
- Uma CPU relativamente capaz. A decodificação por hardware pode ajudar se a CPU for muito lenta
  para decodificar video em tempo real, mas dever ser explicitamente ativada com a opção
  `--hwdec`.
- Uma GPU não muito ruim. O foco de mpv não é reprodução eficiente em energia em
  GPUs incorporadas ou integradas (por exemplo, decodificação por hardware nem sequer
  está ativada como padrão). GPUs com baixa capacidade podem causar problemas como tearing, travamentos,
  etc. A saída principal de video utiliza shaders para renderização de video e redimensionamento,
  em vez de hardware de função fixa da GPU. No Windows, você pode querer se
  certificar que os drivers gráficos são atuais. Em alguns casos, métodos de saída de vídeo antigos
  de contingência podem ajudar (como `--vo=xv` no Linux), mas esse uso não é
  recomendado nem suportado.

## Downloads


Para compilações semioficiais e pacotes de terceiros, consulte
[mpv.io/installation](https://mpv.io/installation/).

## Registo de alterações


Não há nenhum registro completo de alterações; mas, mudanças a base da interface do reprodutor
estão listados na [interface de registo de alterações][mudanças-de-interface].

Alterações a API C são documentadas no [registo de alterações da API do cliente][mudanças-de-api].

A [lista de atualizações][atualizações] tem um sumario das mudanças mais importantes
em cada atualização.

Mudanças as teclas de atalho padrão são indicadas em
[restore-old-bindings.conf][restore-old-bindings].

## Compilação


Compilar com todos os recursos requer arquivos de desenvolvimento para varias
bibliotecas externas. Um dos dois sistemas de compilação suportados pelo mpv são necessários:
[meson](https://mesonbuild.com/index.html) or [waf](https://waf.io/). Meson
pode ser obtido do seu distro ou PyPI. Waf pode ser baixado usando o
`./bootstrap.py` script. Recebera a ultima versão de waf que foi testada
com mpv. Documentações sobre as diferenças entre versões de sistemas estão
localizadas em [build-system-differences][build-system-differences].

**Nota**: Compilação com waf é considerada como *descontinuada* e será removida no
futuro.

### Meson

Após criar o diretório de compilação (por exemplo, `meson setup build`), você pode visualizar uma lista
de todas as opções de compilação através do `comando meson configure build`. Você também pode simplesmente
olhar no arquivo `meson_options.txt`. Os registros são guardados em `meson-logs` dentro
do seu diretório de compilação.

Exemplo:

    meson setup build
    meson compile -C build
    meson install -C build

### Waf

Para obter uma lista das opções de compilação disponíveis use `./waf configure --help`. Se
você pensa que você possui suporte para algum recurso instalado mas a configuração ocorre falha para
detectar, o arquivo `build/config.log` pode conter informação sobre os
motivos para a falha.

NOTA: Para evitar poluir a saída com spam ilegível, `--help` apenas mostra
uma das duas opções para cada opção. Se a opção for auto detectada ou
ativada por padrão, a opção `--disable-***` é impressa; se a opção é
desativada por padrão, a opção `--enable-***` é impressa. De qualquer maneira, você pode
usar `--enable-***` ou `--disable-**` independentemente do que é impresso por `--help`.

Para compilar o programa você pode usar `./waf build`: o resultado da compilação
será localizado em `build/mpv`. Você pode usar `./waf install` para instalar mpv
para o *prefixo* depois de ser compilado.

Exemplo:

    ./bootstrap.py
    ./waf configure
    ./waf
    ./waf install

Dependências essenciais (lista incompleta):

- gcc ou clang
- Cabeçalhos de desenvolvimento X (xlib, xrandr, xext, xscrnsaver, xinerama, libvdpau,
  libGL, GLX, EGL, xv, ...)
- Cabeçalhos de desenvolvimento de saída de áudio (libasound/ALSA, pulseaudio)
- Bibliotecas FFmpeg (libavutil libavcodec libavformat libswscale libavfilter
  e ou libswresample or libavresample)
- zlib
- iconv (normalmente fornecido pela biblioteca de sistema libc)
- libass (OSD, OSC, legendas de texto)
- Lua (opcional, requerido para a pseudo interface gráfica OSC e integração de youtube-dl)
- libjpeg (opcional, usado para capturas de tela apenas)
- uchardet (opcional, para detecção de conjunto de caracteres de legendas)
- Bibliotecas nvdec e vaapi para decodificação de hardware no Linux (opcional)

Libass dependências (ao compilar libass):

- gcc ou clang, yasm em x86 e x86_64
- Cabeçalhos de desenvolvimento fribidi, freetype, fontconfig (para libass)
- harfbuzz (requerido para renderização correta de caracteres combinados, especialmente
  para a renderização correta de texto não inglês no OSX, e scripts árabes/indianos em
  qualquer plataforma)

FFmpeg dependências (ao compilar FFmpeg):

- gcc ou clang, yasm em x86 e x86_64
- OpenSSL ou GnuTLS (precisam ser explicitamente ativados ao compilar FFmpeg)
- libx264/libmp3lame/libfdk-aac se você deseja usar codificação (precisa ser
  explicitamente ativado ao compilar FFmpeg)
- Para reprodução nativa de DASH, o FFmpeg precisa ser compilado com --enable-libxml2
  (embora existam implicações a segurança, e o suporte a DASH tenha muitos bugs).
- Suporte a decodificação AV1 requer dav1d.
- Para bom suporte a nvidia no Linux, tenha certeza que nv-codec-headers está instalado
  e podem ser encontrados por configure.

A maioria das bibliotecas acima estão disponíveis em versões adequadas em distribuições
Linux normais. Para possuir facilidade ao compilar o ultimo git mestre de tudo,
você pode optar por usar um wrapper de compilação disponível diferente ([mpv-compilação][mpv-compilação])
que primeiro compila as bibliotecas do FFmpeg e libass e, em seguida, compila o reprodutor
vinculado estaticamente a estes.

Se você deseja compilar um executável do Windows, você pode usar MSYS2 e MinGW,
ou realizar a compilação cruzada a partir do Linux usando o MinGW. Consulte
[compilação Windows][compile_windows].


## Ciclo de atualização

A cada dois meses, é feito um snapshot arbitrário do git, ao qual é atribuído
um número de versão 0.X.0. Nenhuma manutenção adicional é realizada.

O objetivo de atualizações é tornar as distribuições do Linux felizes. Distribuições Linux
também são esperados a aplicar suas próprias correções no caso de bugs ou problemas de
segurança.

Atualizações diferentes da mais atual não são suportadas ou recebem manutenção.

Consulte o [documento de política de atualização][política-de-atualização] para mais informações.

## Reportagem de bugs


Por favor utilize o [rastreador de problemas][rastreador-de-problemas] fornecido pelo GitHub para nos enviar relatórios
de bugs ou solicitações de recursos. Siga as instruções de formatação ou o problema
provavelmente será ignorado ou fechado como inválido.

Usar o rastreador de problemas como um lugar para fazer perguntas simples é aceitável mas é
recomendado usar o IRC (consulte [Contato](#Contato) abaixo).

## Contribuir


Por favor leia [contribute.md][contribute.md].

Para pequenas alterações você pode simplesmente fazer uma solicitação pelo GitHub. Para mudanças
maiores venha conversar conosco no IRC antes de começar a trabalhar nelas. Isto facilitará
a avaliação do código para ambos os grupos posteriormente.

Você pode verificar [a wiki](https://github.com/mpv-player/mpv/wiki/Stuff-to-do)
ou o [rastreador de problemas](https://github.com/mpv-player/mpv/issues?q=is%3Aopen+is%3Aissue+label%3Ameta%3Afeature-request)
por ideias sobre o que você pode contribuir.

## Licença

GPLv2 "ou posterior" por padrão, LGPLv2.1 "ou posterior" com `--enable-lgpl`.
Consulte [detalhes.](https://github.com/mpv-player/mpv/blob/master/Copyright)

## História

Este programa é baseado no projeto MPlayer. Antes de mpv existir como um projeto,
a base do código foi brevemente desenvolvida no contexto do projeto mplayer2. Para mais detalhes,
consulte as [perguntas frequentes][perguntas-frequentes].

## Contato


A maioria da atividade ocorre no canal IRC e no rastreador de problemas do github.

- **GitHub rastreador de problemas**: [rastreador de problemas][rastreador-de-problemas] (reporte bugs aqui)
- **Canal IRC de usuários**: `#mpv` on `irc.libera.chat`
- **Canal IRC de desenvolvedores**: `#mpv-devel` on `irc.libera.chat`

[perguntas-frequentes]: https://github.com/mpv-player/mpv/wiki/FAQ
[atualizações]: https://github.com/mpv-player/mpv/releases
[mpv-compilação]: https://github.com/mpv-player/mpv-build
[rastreador-de-problemas]:  https://github.com/mpv-player/mpv/issues
[política-de-atualização]: https://github.com/mpv-player/mpv/blob/master/DOCS/release-policy.md
[compile_windows]: https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md
[mudanças-de-interface]: https://github.com/mpv-player/mpv/blob/master/DOCS/interface-changes.rst
[mudanças-de-api]: https://github.com/mpv-player/mpv/blob/master/DOCS/client-api-changes.rst
[restore-old-bindings]: https://github.com/mpv-player/mpv/blob/master/etc/restore-old-bindings.conf
[contribute.md]: https://github.com/mpv-player/mpv/blob/master/DOCS/contribute.md
[build-system-differences]: https://github.com/mpv-player/mpv/blob/master/DOCS/build-system-differences.md
