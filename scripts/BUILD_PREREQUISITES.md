# Pré-requisitos de build

Esta lista registra os pacotes de desenvolvimento para Debian/Ubuntu x86-64,
o caminho alternativo MSYS2 e o estado atual dos builds.

## (a) Linux — Debian/Ubuntu

O conjunto inclui CMake/Ninja, Qt Widgets, FFmpeg, libebml/libmatroska,
MKVToolNix CLI, testes com GoogleTest e ferramentas para validar/empacotar.
O build do MKVToolNix usa --disable-gui; as bibliotecas Qt abaixo são para o
Musubi e para componentes do build, não adicionam módulos Qt à aplicação.

    sudo apt update
    sudo apt install \
      build-essential cmake ninja-build pkg-config git curl ca-certificates \
      xz-utils bzip2 tar gpg gpgv autoconf automake libtool make patch \
      nasm yasm binutils file patchelf squashfs-tools desktop-file-utils \
      python3 perl ruby rake gettext po4a xsltproc docbook-xsl gperf \
      libboost-dev libboost-date-time-dev libboost-filesystem-dev \
      libboost-math-dev libboost-regex-dev libboost-system-dev \
      libbz2-dev libcmark-dev libdvdread-dev libflac-dev libfmt-dev \
      libgmp-dev libgtest-dev liblzo2-dev libmagic-dev libogg-dev \
      libpcre2-dev libvorbis-dev nlohmann-json3-dev zlib1g-dev \
      qt6-base-dev qt6-base-dev-tools

Para compilar também os componentes Windows a partir do Linux com MinGW, os
pacotes abaixo são opcionais. O caminho recomendado para Musubi.exe é o
build nativo Windows descrito na seção (b).

    sudo apt install mingw-w64 g++-mingw-w64-x86-64-posix

## (b) Windows — MSYS2/MinGW-w64, alternativa

No terminal MSYS2 UCRT64, instale:

    pacman -S --needed \
      base-devel git curl ca-certificates gnupg tar xz \
      mingw-w64-ucrt-x86_64-toolchain \
      mingw-w64-ucrt-x86_64-cmake \
      mingw-w64-ucrt-x86_64-ninja \
      mingw-w64-ucrt-x86_64-qt6-base \
      mingw-w64-ucrt-x86_64-nasm \
      mingw-w64-ucrt-x86_64-yasm \
      mingw-w64-ucrt-x86_64-pkgconf \
      mingw-w64-ucrt-x86_64-autotools

Esse caminho compila com MinGW/UCRT e pode exigir que libgcc, libstdc++,
libwinpthread e as DLLs do UCRT acompanhem o pacote. Não misture DLLs de
MinGW com um executável MSVC.

## FFmpeg no Windows com MSVC

O FFmpeg será compilado do mesmo código-fonte e versão fixada no Linux,
usando `--toolchain=msvc --enable-shared --disable-static`. O FFmpeg gera
DLLs; os arquivos `.def` correspondentes serão usados com `lib.exe /DEF` para
gerar import libraries `.lib` compatíveis com o linker do MSVC. O script
`build_ffmpeg_msvc.sh` faz essa etapa e valida que os executáveis, DLLs e
import libraries foram produzidos. O fluxo oficial do FFmpeg para MSVC usa
MSYS2 apenas como shell/ferramentas POSIX (`make` e NASM), iniciado a partir do
Developer Command Prompt do Visual Studio para herdar `cl.exe`, `link.exe` e
`lib.exe`.

No shell MSYS2 MSYS (não UCRT64), os pacotes auxiliares são:

    pacman -S --needed base-devel coreutils make nasm git curl gnupg tar xz

| Opção | Resultado e compatibilidade | Avaliação |
|---|---|---|
| FFmpeg via MSYS2 UCRT64/MinGW-w64 | DLLs e `.dll.a`; para linkar no MSVC é preciso converter os `.def` em `.lib` e controlar runtimes/ABI | Funciona, mas adiciona conversão e runtime MinGW ao pacote |
| vcpkg com triplet dinâmico `x64-windows` | Compila com MSVC e seu port converte `.def` em `.lib`; tem features para `ffmpeg.exe` e `ffprobe.exe` | Alternativa válida; exige manter baseline/manifest do vcpkg alinhados à versão FFmpeg do Linux |
| FFmpeg upstream com `--toolchain=msvc` (escolhida) | Usa a versão fixada pelo projeto, produz DLLs e `.def`; `lib.exe` gera import `.lib` | Menos camadas e mesmo código/configuração nos dois alvos; script próprio valida os artefatos |

Em todos os alvos, o FFmpeg deve ser configurado sem `--enable-gpl`,
`--enable-version3` ou `--enable-nonfree`, com detecção automática de
dependências desativada. Os codecs extras seguem desligados. O build Linux
registrado acima foi concluído; o build FFmpeg específico para Windows ainda
não foi executado.

## (c) Windows — caminho recomendado para Musubi.exe

- Visual Studio 2022 Community ou Build Tools, com o workload Desktop
  development with C++.
- CMake 3.20+ e Ninja.
- Qt 6 para MSVC 2022 x64, com apenas Core, Gui e Widgets instalados.
- A compilação deve ser iniciada no Developer PowerShell/Command Prompt do
  Visual Studio, com cl.exe disponível.

O build da aplicação usa o Qt MSVC instalado e o script
build_cpp_windows.bat. A aplicação chama `ffmpeg`, `ffprobe` e `mkvmerge` como
processos externos e, por isso, não liga diretamente às bibliotecas libav nem
precisa das import libraries `.lib` do FFmpeg. O script
build_ffmpeg_msvc.sh prepara a distribuição dinâmica do FFmpeg (executáveis e
DLLs) quando ela for necessária; vcpkg permanece opcional. O utilitário
windeployqt reúne as DLLs necessárias e o plugin de plataforma do Qt; o script
omite plugins opcionais de rede para manter o deployment nos módulos usados.

## Comparação Windows

| Caminho | Vantagem | Custo/risco |
|---|---|---|
| MSVC 2022 + Qt MSVC + FFmpeg upstream via MSVC (recomendado) | Toolchain nativa, DLLs e `.lib` MSVC, deployment do Qt por windeployqt | Exige Visual Studio Build Tools, Qt MSVC e o shell/ferramentas auxiliares do MSYS2 |
| MSYS2 UCRT64 + MinGW-w64 | Bash e ferramentas GNU nativas no Windows; útil para bibliotecas com build POSIX | Mais DLLs de runtime para empacotar; mantenha todo componente linkado no mesmo ABI |
| Cross-compile em Linux | Pode automatizar targets sem uma máquina Windows dedicada | Requer toolchain e Qt Windows correspondentes; o build Windows do MKVToolNix é uma etapa separada e frágil. Não é o caminho escolhido para o .exe |

Escolha atual: compilar Musubi.exe nativamente com MSVC 2022 e Qt MSVC.
Os executáveis de mídia continuam sendo processos separados. A viabilidade de
compilar MKVToolNix Windows do código-fonte será avaliada separadamente; não
será prometido um mkvmerge.dll.

## Estado de validação do ambiente

- Verificado em 2026-10-04: Ubuntu 24.04 está instalado no WSL2 (versão 2).
- Build Linux de dependências concluído no clone `~/src/CFSB-MUXER`: libebml
  1.4.7, libmatroska 1.7.2, FFmpeg 9.0.2 em configuração LGPL e MKVToolNix
  102.0 somente CLI. Os comandos `ffmpeg -version`, `ffprobe -version`,
  `mkvmerge --version` e `ldd` foram usados na validação. O build durou 45m39s
  e `build/` mais `dist/` ocuparam aproximadamente 1,08 GiB.
- Pendente: empacotamento AppImage e inicialização em Linux nativo. A validação
  final do AppImage permanece para o computador Linux antigo.
- Verificado em 2026-10-04: Visual Studio Build Tools 2022 instalado em
  `C:\VS2022BuildTools`; MSVC x64 19.44.35229, CMake 3.31.6 e Ninja 1.12.1
  respondem no Developer Command Prompt.
- Verificado em 2026-10-04: aplicação C++ compilada em Release com MSVC
  19.44.35229.0, CMake 3.31.6, Ninja 1.12.1 e Qt 6.12.0 MSVC 2022 x64.
  `dist/windows/Musubi.exe` e as dependências Qt foram geradas pelo script
  `scripts/build_cpp_windows.bat`; o `windeployqt` reportou sucesso e o app foi
  iniciado para inspeção visual. O teste CTest do parser de capítulos passou.
- A execução de mux de ponta a ponta gerou MKV, JSON e thumbnail. A thumbnail
  foi idêntica à referência; o JSON coincidiu exceto pelo CRC. O CRC do MKV
  variou por UIDs Matroska gerados a cada mux, então a saída binária completa
  não foi idêntica. A decisão sobre UIDs determinísticos permanece pendente.
- O CMake liga a aplicação somente a Qt Core, Gui e Widgets. O script de build
  pula plugins opcionais de rede no `windeployqt`.
- O FFmpeg que já está no PATH do Windows reporta
  `--enable-gpl --enable-version3`; ele não deve ser redistribuído no pacote
  LGPL do projeto. FFmpeg LGPL para Windows e ferramentas CLI ainda não estão
  incluídos em `dist/windows`.

O clone WSL fica em `/home/<usuario>/src/CFSB-MUXER`, fora de `/mnt/c` e
`/mnt/d`. Edite no checkout Windows; no clone Linux, atualize somente com
`git fetch` seguido de `git merge --ff-only`. A validação final do AppImage
deve ocorrer em Linux nativo.

## Estimativa de build completo

Estimativa inicial, sujeita a CPU, armazenamento e velocidade de download:

- Linux: 30–90 minutos e 8–15 GB livres para fontes, árvores temporárias,
  logs e prefixo de distribuição.
- Windows: 20–60 minutos e 6–12 GB livres para FFmpeg, dependências, build C++
  e pacote; o MKVToolNix será tratado como executáveis externos, sem DLL fictícia.
