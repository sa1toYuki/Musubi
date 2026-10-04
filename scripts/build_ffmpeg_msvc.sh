#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
export DIST_PREFIX_OVERRIDE="${REPO_ROOT}/dist/windows"
source "${SCRIPT_DIR}/build_common.sh"
unset DIST_PREFIX_OVERRIDE

[[ "${MSYSTEM:-}" == "MSYS" ]] ||
  fail "Abra o shell MSYS2 MSYS a partir do Developer Command Prompt do Visual Studio."
require_commands make cl.exe link.exe lib.exe nasm.exe cygpath nproc

SOURCE_DIR="${SOURCE_ROOT}/ffmpeg-${FFMPEG_VERSION}"
BUILD_DIR="${BUILD_ROOT_DIR}/ffmpeg-${FFMPEG_VERSION}-msvc"
LOGFILE="${LOG_DIR}/ffmpeg-${FFMPEG_VERSION}-msvc.log"
[[ -x "${SOURCE_DIR}/configure" ]] || fail "Execute fetch_sources.sh primeiro."
mkdir -p "${BUILD_DIR}" "${DIST_PREFIX}/bin" "${DIST_PREFIX}/lib" "${DIST_PREFIX}/include"

(
  cd "${BUILD_DIR}"
  log_command "${LOGFILE}" "${SOURCE_DIR}/configure" \
    --prefix="${DIST_PREFIX}" --bindir="${DIST_PREFIX}/bin" \
    --libdir="${DIST_PREFIX}/lib" --incdir="${DIST_PREFIX}/include" \
    --arch=x86_64 --target-os=win32 --toolchain=msvc \
    --enable-shared --disable-static --enable-pic --disable-autodetect \
    --disable-gpl --disable-version3 --disable-nonfree --disable-doc \
    --extra-cflags=-MD

  if grep -Eq '^CONFIG_(GPL|VERSION3|NONFREE)=yes$' config.mak; then
    fail "Configuração FFmpeg ativou GPL, version3 ou nonfree."
  fi

  log_command "${LOGFILE}" make --jobs="${JOBS}"
  log_command "${LOGFILE}" make install
)

shopt -s nullglob
def_files=("${DIST_PREFIX}/lib/"*.def)
(( ${#def_files[@]} > 0 )) || fail "FFmpeg não instalou arquivos .def para import libraries."
for def_file in "${def_files[@]}"; do
  def_name="$(basename -- "${def_file}" .def)"
  library_name="${def_name%-[0-9]*}"
  library_name="${library_name#lib}"
  def_native="$(cygpath --windows "${def_file}")"
  library_native="$(cygpath --windows "${DIST_PREFIX}/lib/${library_name}.lib")"
  log_command "${LOGFILE}" lib.exe /machine:x64 "/def:${def_native}" "/out:${library_native}"
done

[[ -x "${DIST_PREFIX}/bin/ffmpeg.exe" ]] || fail "ffmpeg.exe não foi instalado."
[[ -x "${DIST_PREFIX}/bin/ffprobe.exe" ]] || fail "ffprobe.exe não foi instalado."
dll_files=("${DIST_PREFIX}/bin/"*.dll)
import_libraries=("${DIST_PREFIX}/lib/"*.lib)
(( ${#dll_files[@]} > 0 )) || fail "Nenhuma DLL do FFmpeg foi instalada."
(( ${#import_libraries[@]} > 0 )) || fail "Nenhuma import library .lib foi gerada."

"${DIST_PREFIX}/bin/ffmpeg.exe" -version | tee -a "${LOGFILE}"
"${DIST_PREFIX}/bin/ffprobe.exe" -version | tee -a "${LOGFILE}"
printf 'FFmpeg MSVC: %d DLL(s), %d import library/libraries em %s\n' \
  "${#dll_files[@]}" "${#import_libraries[@]}" "${DIST_PREFIX}" | tee -a "${LOGFILE}"
