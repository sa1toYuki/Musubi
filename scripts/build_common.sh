#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
source "${SCRIPT_DIR}/config.env"
BUILD_ROOT="${REPO_ROOT}/build/media"
SOURCE_ROOT="${BUILD_ROOT}/sources"
BUILD_ROOT_DIR="${BUILD_ROOT}/work"
DIST_PREFIX="${DIST_PREFIX_OVERRIDE:-${REPO_ROOT}/dist/linux}"
LOG_DIR="${REPO_ROOT}/build/logs"
JOBS="${BUILD_JOBS:-$(nproc)}"
mkdir -p "${SOURCE_ROOT}" "${BUILD_ROOT_DIR}" "${DIST_PREFIX}" "${LOG_DIR}"

fail() {
  printf 'ERRO: %s\n' "$*" >&2
  exit 1
}

require_commands() {
  local name
  for name in "$@"; do
    command -v "${name}" >/dev/null 2>&1 || fail "Ferramenta ausente: ${name}"
  done
}

log_command() {
  local logfile="$1"
  shift
  mkdir -p "$(dirname -- "${logfile}")"
  printf '+'
  printf ' %q' "$@"
  printf '\n'
  "$@" 2>&1 | tee -a "${logfile}"
}

setup_native_pkg_config() {
  require_commands pkg-config
  local system_pc_path
  system_pc_path="$(pkg-config --variable=pc_path pkg-config)"
  # Native Linux build only. Cross-target builds require a separate sysroot.
  export PKG_CONFIG_LIBDIR="${DIST_PREFIX}/lib/pkgconfig:${DIST_PREFIX}/lib64/pkgconfig:${DIST_PREFIX}/share/pkgconfig:${system_pc_path}"
  export PKG_CONFIG_PATH="${DIST_PREFIX}/lib/pkgconfig:${DIST_PREFIX}/lib64/pkgconfig:${DIST_PREFIX}/share/pkgconfig"
}
