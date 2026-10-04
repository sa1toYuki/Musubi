# Musubi

Multiplexador MKV. Combina vídeo, legenda `.ass`, capítulos e fontes opcionais em um `.mkv` final com CRC-32 no nome — no padrão de releases de anime.

Interface gráfica inclusa. A tag da fansub é configurável (padrão: `CFSB`).

## Requisitos

No Windows, o ZIP do aplicativo inclui Qt, mas não inclui FFmpeg nem MKVToolNix.
Instale versões compatíveis dessas ferramentas e deixe `ffmpeg`, `ffprobe` e
`mkvmerge` no `PATH` (ou nos caminhos padrão):

| Ferramenta | Função |
|---|---|
| [MKVToolNix](https://mkvtoolnix.download/) | `mkvmerge` / `mkvinfo` |
| [FFmpeg](https://ffmpeg.org/download.html) | Thumbnail e duração (`ffmpeg` / `ffprobe`) |

Caminhos detectados automaticamente no Windows:

- `C:\Program Files\MKVToolNix\`
- `C:\ffmpeg\bin\`

No AppImage Linux, `scripts/build_all.sh` instala as dependências dinâmicas em
`dist/linux`; o empacotador as inclui no AppImage. Ainda assim, valide o pacote
na distro Linux de destino.

## Download (Releases)

Baixe `Musubi-windows-x64.zip` na aba **Releases** do GitHub. O pacote contém `Musubi.exe` e as DLLs necessárias.

## Uso da interface gráfica

1. Extraia o ZIP e execute `Musubi.exe`.
2. Selecione vídeo (`.mkv`) e legenda (`.ass`). Por padrão, os capítulos são
   extraídos automaticamente da legenda; um `.txt` é opcional para fallback.
3. Escolha a origem dos capítulos: **Automático pela legenda** (padrão),
   **Arquivo .txt** ou **Sem capítulos**.
4. Opcional: pasta de fontes (`.ttf` / `.otf`) e pasta de saída.
5. Informe **tag da fansub**, anime, episódio e source.
6. Marque JSON e/ou thumbnail se desejar.
7. Clique em **Multiplexar**.

No Windows, execute `Musubi.exe` dentro da pasta extraída do ZIP. No Linux,
execute `chmod +x Musubi-x86_64.AppImage` uma vez e abra o AppImage pelo
gerenciador de arquivos ou rode `./Musubi-x86_64.AppImage` no terminal. Para
usar dependências externas no Windows, confirme antes que `ffmpeg`, `ffprobe`
e `mkvmerge` respondem no terminal. A ausência de `mkvinfo` é apenas um aviso.

Arquivo gerado:

```
[SUA_TAG] Nome do Anime - 05 [1080p][WEB-DL][AVC][AAC][A1B2C3D4].mkv
```

## Compilar no Windows

### Pré-requisitos

- Visual Studio 2022 ou Build Tools com o workload **Desktop development with C++**.
- CMake 3.20+ e Ninja (podem ser instalados pelos componentes do Visual Studio).
- Qt 6 para **MSVC 2022 64-bit**, incluindo Qt Base (Core, Gui e Widgets).
- Inkscape 1.4.4 só é necessário para regenerar os ícones a partir do SVG.

Veja também a lista detalhada em
[`scripts/BUILD_PREREQUISITES.md`](scripts/BUILD_PREREQUISITES.md), seção
“Windows — caminho recomendado”.

### Passos

1. Abra o **Developer PowerShell for VS 2022**.
2. Entre na raiz do clone e, se o Qt estiver fora dos caminhos detectados pelo
   script, configure `QT_MSVC_DIR` para a pasta do kit MSVC x64:

```powershell
cd D:\Programas\CFSB-MUXER
$env:QT_MSVC_DIR = 'C:\Qt\6.12.0\msvc2022_64'
.\scripts\build_cpp_windows.bat
```

Se necessário, regenere os PNGs/ICO com `.\scripts\generate_icons.ps1` antes
do build. O script CMake compila, instala as DLLs do Qt via `windeployqt` e
cria `dist\Musubi-windows-x64.zip`. O executável fica em
`dist\windows\Musubi.exe`. FFmpeg e MKVToolNix são dependências externas no
Windows e precisam estar no `PATH`.

## Compilar no Linux e criar AppImage

O build deve ser feito em Ubuntu 24.04 x86-64, nativamente ou no WSL2. No WSL,
mantenha o clone no sistema de arquivos Linux, por exemplo
`/home/<usuario>/src/CFSB-MUXER`, e não em `/mnt/c` ou `/mnt/d`.

1. Instale os pacotes da seção “Linux — Debian/Ubuntu” em
   [`scripts/BUILD_PREREQUISITES.md`](scripts/BUILD_PREREQUISITES.md).
2. Na raiz do clone, compile libebml, libmatroska, FFmpeg (configuração LGPL),
   MKVToolNix CLI e o aplicativo:

   ```bash
   bash scripts/build_all.sh linux
   ```

   O script registra logs em `build/logs/` e instala em `dist/linux/`.
3. Para gerar o AppImage, baixe os AppImages x86-64 oficiais do
   [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy/releases), do
   [plugin Qt](https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases)
   e do [appimagetool](https://github.com/AppImage/appimagetool/releases).
   Coloque os três AppImages em `~/bin`, ao lado uns dos outros, e torne-os
   executáveis. O plugin Qt deve ser reconhecível como
   `linuxdeploy-plugin-qt`; crie links com os nomes `linuxdeploy`,
   `linuxdeploy-plugin-qt` e `appimagetool` se os arquivos baixados incluírem
   sufixos de arquitetura/versão. Inclua `~/bin` no `PATH`.
4. Empacote:

   ```bash
   bash scripts/build_appimage.sh
   ```

O resultado é `dist/Musubi-x86_64.AppImage`. O build das dependências Linux foi
feito no WSL2; o empacotamento AppImage e a execução em Linux nativo ainda
precisam de validação. Teste o resultado no computador Linux de destino antes
de publicar. A lista de pacotes, alternativas Windows e estado dos testes está
em [`scripts/BUILD_PREREQUISITES.md`](scripts/BUILD_PREREQUISITES.md).

## Publicar no GitHub Releases

1. Atualize a versão em `cpp/CMakeLists.txt`.
2. Crie e envie uma tag:

```bat
git tag v1.0.0
git push origin v1.0.0
```

3. O workflow `.github/workflows/release.yml` publica `Musubi.exe` e `Musubi-windows-x64.zip` na release.

## Formato dos capítulos

Na versão C++, o modo automático reconhece somente linhas `Comment:` na seção
`[Events]` cujo campo `Effect` seja `CAP`, `Capítulo`, `Capitulo`, `Chapter` ou
`Chap` (sem diferenciar maiúsculas de minúsculas). `Start` define o tempo e
`Text` define o nome. Se a legenda não tiver capítulos reconhecidos, o `.txt`
informado é usado como fallback; sem fallback, o mux segue sem capítulos.
O modo `.txt` continua aceitando o formato abaixo.

```
CHAPTER01=00:00:00.000
CHAPTER01NAME=Abertura
CHAPTER02=00:01:30.000
CHAPTER02NAME=Parte A
```

## Licença

O código do Musubi é distribuído sob a GNU GPL versão 3 ou posterior; veja
o texto integral em [`LICENSE`](LICENSE). Você pode usar, modificar e redistribuir
o projeto, inclusive comercialmente, cumprindo os termos da GPL.

Feito por fãs, para fãs. Use, modifique e compartilhe à vontade.

## Agradecimento

A build original em Python teve como referência o repositório
[CFSB-MKV-Muxer, de Kelvao](https://github.com/Kelvao/CFSB-MKV-Muxer).
Obrigado por disponibilizar o projeto que ajudou a dar início a este trabalho.

## Créditos e componentes de terceiros

- [FFmpeg](https://ffmpeg.org/) — ferramenta de mídia. A configuração de build
  do projeto mantém os codecs extras desativados e não usa as opções GPL,
  version3 ou nonfree.
- [MKVToolNix](https://mkvtoolnix.download/) — fornece `mkvmerge` e, quando
  instalado, `mkvinfo`.
- [libebml](https://github.com/Matroska-Org/libebml) e
  [libmatroska](https://github.com/Matroska-Org/libmatroska) — bibliotecas
  usadas pelo MKVToolNix.
- [Qt](https://www.qt.io/) — interface C++ (módulos Core, Gui e Widgets).

Os scripts de [`scripts/`](scripts/) baixam o código-fonte dos componentes e
preparam seus builds; veja em especial `fetch_sources.sh`, `build_matroska.sh`,
`build_ffmpeg.sh`, `build_ffmpeg_msvc.sh` e `build_mkvtoolnix.sh`. Os avisos de
licença e as fontes oficiais estão resumidos em
[`THIRD_PARTY_LICENSES`](THIRD_PARTY_LICENSES).

## Configuração e logs

O nome de aplicativo do Qt é `Musubi`; a localização padrão de configuração
passa a ser `%APPDATA%/Crystal FanSub/Musubi` no Windows e
`~/.config/Crystal FanSub/Musubi` no Linux. A aplicação atual não grava
preferências nessa pasta. Também não mantém um arquivo de log automático: o
log é exibido na janela e qualquer exportação é salva no caminho escolhido
pelo usuário. Portanto, não há dados da antiga pasta `Muxer Pro` para migrar.
