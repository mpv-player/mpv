Compilar para Windows
=====================

Compilação para Windows é suportada com MinGW-w64. Isto pode ser usado para produzir
ambos executáveis de 32-bit e 64-bit, e funciona para compilar no Windows e
fazer compilação cruzada do Linux para Cygwin. MinGW-w64 está disponível em:
https://www.mingw-w64.org/

Embora seja possível construir uma ferramenta completa do MinGW-w64 por conta própria, há alguns
ambientes de compilação e scripts disponíveis para facilitar o processo, como o MSYS2 e
MXE. Observe que os ambientes MinGW incluídos nas distribuições Linux geralmente possuem
problemas, estão desatualizados e são inúteis, e normalmente não utilizam o MinGW-w64.

**Aviso**: o MinGW original (https://osdn.net/projects/mingw/) não é suportado.

Compilação cruzada
==================

Ao realizar uma compilação cruzada, é recomendado usar um arquivo de configuração Meson para configurar
o ambiente de compilação cruzada. Abaixo está um exemplo simples:

```ini
[binaries]
c = 'x86_64-w64-mingw32-gcc'
cpp = 'x86_64-w64-mingw32-g++'
ar = 'x86_64-w64-mingw32-ar'
strip = 'x86_64-w64-mingw32-strip'
exe_wrapper = 'wine64'

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
```

Consulte a [documentação do meson](https://mesonbuild.com/Cross-compilation.html) para
mais informações.

[MXE](https://mxe.cc) facilita muito a inicialização de um ambiente completo do MinGW-w64
a partir de uma máquina Linux. Veja um exemplo funcional abaixo.

Alternativamente, você pode tentar [mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake),
que inicializa um ambiente MinGW-w64 e compila mpv e suas dependências.

Exemplo com MXE
----------------

```bash
# Antes de começar, certifique-se de instalar os pré-requisitos do MXE. O MXE irá baixar
# e compilar todas as dependências do destino, mas não as dependências do host. Por exemplo,
# você precisa ter um compilador funcional, ou o MXE não conseguirá construir o compilador cruzado.
#
# Consulte
#
#    https://mxe.cc/#requirements
#
# Vá para baixo na página e encontre as instruções especificas de distro/OS Scroll e instale eles.

# Baixe MXE. Observe que compilar os pacotes requeridos requer em torno de 1.4GB
# ou mais!

cd /opt
git clone https://github.com/mxe/mxe mxe
cd mxe

# Define as opções de compilação.

# A variável de ambiente JOBS controla o número de threads a serem usados durante a compilação. NÃO
# use a opção regular `make -j4` com o MXE, pois isso pode retardar a compilação.
# Alternativamente, você pode definir isso no comando make adicionando "JOBS=4
# ao final do comando:
echo "JOBS := 4" >> settings.mk

# A variável de ambiente MXE_TARGET é usada para compilar o MinGW-w64 para destinos de 32 bits.
# Alternativamente, você pode especificar isso no comando make adicionando
# "MXE_TARGETS=i686-w64-mingw32" ao final do comando:
echo "MXE_TARGETS := i686-w64-mingw32.static" >> settings.mk

# Se você deseja compilar a versão de 64 bits, use isto:
# echo "MXE_TARGETS := x86_64-w64-mingw32.static" >> settings.mk

# Compile os pacotes necessários. Os seguintes pacotes fornecem o mínimo necessário para compilar um
# binário razoável do mpv (embora não seja o mínimo absoluto).

make gcc ffmpeg libass jpeg lua luajit

# Adicione MXE binários para $PATH
export PATH=/opt/mxe/usr/bin/:$PATH

# Compile o mpv. O destino será usado para selecionar automaticamente o nome das
# ferramentas de compilação envolvidas (por exemplo, ele usará i686-w64-mingw32.static-gcc).

cd ..
git clone https://github.com/mpv-player/mpv.git
cd mpv
meson setup build --crossfile crossfile
meson compile -C build
```

Compilação nativa com MSYS2
===========================

Para desenvolvedores Windows que desejam começar rapidamente, o MSYS2 pode ser usado
para compilar o mpv nativamente em uma máquina Windows. Os repositórios do MSYS2 possuem pacotes
binários para a maioria das dependências do mpv, então o processo deve envolver apenas
a compilação do próprio mpv.

Para compilar mpv 64-bit no Windows:

Instalando MSYS2
----------------

1. Baixe um instalador do site https://www.msys2.org/

   Ambos o i686 e a versão x86_64 do MSYS2 podem compilar binários mpv
   32-bit e 64-bit quando rodam em uma versão 64-bit do Windows, mas a versão
   x86_64 é preferível sendo que espaços de endereços maiores fazem com que seja menos propenso
   a erros de fork().

2. Inicie um shell do MinGW-w64 (``mingw64.exe``). **Observação*8: Isso é diferente
   o shell do MSYS2 que é iniciado a partir da última caixa de diálogo de instalação. Você deve
   fechar esse shell e abrir um novo.

   For a 32-bit build, use ``mingw32.exe``.

Atualizando MSYS2
-----------------

Para prevenir erros durante a pós instalação, a execução principal do MSYS2 deve ser atualizada
separadamente.

```bash
# Verifique se há atualizações do núcleo. Se for instruído, feche a janela do shell e abra-a novamente
# antes de continuar.
pacman -Syu

# Atualize todo o resto
pacman -Su
```

Instalando dependências mpv
---------------------------

```bash
# Instale as dependências de compilação do MSYS2 e um compilador MinGW-w64
pacman -S git $MINGW_PACKAGE_PREFIX-{python,pkgconf,gcc,meson}

# Instale as dependências mais importantes do MinGW-w64. libass e lcms2 também são
# incluídos como dependências do ffmpeg.
pacman -S $MINGW_PACKAGE_PREFIX-{ffmpeg,libjpeg-turbo,luajit}
```

Compilando mpv
--------------

Por fim, compile e instale o mpv. Os binários serão instalados em
``/mingw64/bin`` or ``/mingw32/bin``.

```bash
meson setup build --prefix=$MSYSTEM_PREFIX
meson compile -C build
```

Ou compile e instale tanto o libmpv quanto o mpv:

```bash
meson setup build -Dlibmpv=true --prefix=$MSYSTEM_PREFIX
meson compile -C build
meson install -C build
```

Vinculando o libmpv com programas MSVC
--------------------------------------

O mpv/libmpv não pode ser construído com o Visual Studio (a Microsoft é incompetente demais para oferecer
suporte adequado ao C99/C11 e/ou tem uma aversão muito grande ao código aberto e ao Linux
para fazê-lo seriamente). No entanto, você pode construir programas em C++ no Visual Studio e vinculá-los
com um libmpv construído com o MinGW.

Para fazer isso, você precisa de um Visual Studio que suporte ``stdint.h`` (versões recentes suportam)
e você precisa criar uma biblioteca de importação para a DLL do mpv:

```bash
lib /name:mpv-1.dll /out:mpv.lib /MACHINE:X64
```

A sequência de caracteres no parâmetro "/name:" deve corresponder ao nome do arquivo DLL (este
é apenas o nome de arquivo que o linker do MSVC usará).

A vinculação estática não é possível.

Rodando mpv
-----------

Se você deseja executar o mpv a partir do shell MinGW-w64, terá uma experiência muito
mais agradável se usar a utilidade ``winpty``.

```bash
pacman -S winpty
winpty mpv.com ToS-4k-1920.mov
```

Se você deseja mover/copiar "mpv.exe" e "mpv.com" para algum lugar diferente de
``/mingw64/bin/`` para uso fora do shell MinGW-w64, eles ainda dependerão das
DLLs naquela pasta. A solução mais simples é adicionar ``C:\msys64\mingw64\bin``
ao caminho de sistema ``%PATH%`` do Windows. tenha cuidado, pois isso pode causar problemas ou
confusão no Cygwin se ele também estiver instalado na máquina.

O uso do backend OpenGL ANGLE requer uma cópia da DLL do compilador D3D que
corresponda à versão do SDK D3D com a qual o ANGLE foi construído
(``d3dcompiler_43.dll`` no caso do ANGLE construído com o MinGW) no caminho ou na
mesma pasta do mpv. Ela deve ter a mesma arquitetura (x86_64 / i686) do
mpv que você compilou.
