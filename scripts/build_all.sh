#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/build_common.sh"

build_linux() {
  bash "${SCRIPT_DIR}/fetch_sources.sh" libebml
  bash "${SCRIPT_DIR}/build_matroska.sh" libebml
  bash "${SCRIPT_DIR}/fetch_sources.sh" libmatroska
  bash "${SCRIPT_DIR}/build_matroska.sh" libmatroska
  bash "${SCRIPT_DIR}/fetch_sources.sh" ffmpeg
  bash "${SCRIPT_DIR}/build_ffmpeg.sh"
  bash "${SCRIPT_DIR}/fetch_sources.sh" mkvtoolnix
  bash "${SCRIPT_DIR}/build_mkvtoolnix.sh"
  if [[ -f "${REPO_ROOT}/cpp/CMakeLists.txt" ]]; then
    require_commands cmake ninja
    log_command "${LOG_DIR}/cpp-linux.log" cmake -S "${REPO_ROOT}/cpp" \
      -B "${REPO_ROOT}/build/cpp-linux" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="${DIST_PREFIX}" \
      -DCMAKE_INSTALL_PREFIX="${DIST_PREFIX}"
    log_command "${LOG_DIR}/cpp-linux.log" cmake --build "${REPO_ROOT}/build/cpp-linux" --parallel "${JOBS}"
    log_command "${LOG_DIR}/cpp-linux.log" cmake --install "${REPO_ROOT}/build/cpp-linux"
  else
    printf 'Dependências Linux compiladas; fonte C++ indisponível neste checkout.\n'
  fi
}

target="${1:-linux}"
case "${target}" in
  linux) build_linux ;;
  windows)
    printf 'FFmpeg MSVC: no shell MSYS2 MSYS iniciado pelo Developer Command Prompt, execute bash scripts/fetch_sources.sh e bash scripts/build_ffmpeg_msvc.sh.\n'
    printf 'Aplicativo: scripts/build_cpp_windows.bat no Developer PowerShell com MSVC e Qt MSVC.\n'
    exit 2
    ;;
  all)
    build_linux
    printf 'Etapa Windows nativa pendente: FFmpeg usa scripts/build_ffmpeg_msvc.sh; o app usa scripts/build_cpp_windows.bat.\n'
    ;;
  *)
    printf 'Uso: bash scripts/build_all.sh [linux|windows|all]\n' >&2
    exit 2
    ;;
esac
