#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/build_common.sh"
require_commands cmake ninja
setup_native_pkg_config

build_libebml() {
  local source_dir="${SOURCE_ROOT}/libebml"
  local build_dir="${BUILD_ROOT_DIR}/libebml-${LIBEBML_VERSION}"
  local logfile="${LOG_DIR}/libebml.log"
  [[ -f "${source_dir}/CMakeLists.txt" ]] || fail "Execute 'bash scripts/fetch_sources.sh libebml' primeiro."
  log_command "${logfile}" cmake -S "${source_dir}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${DIST_PREFIX}" \
    -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_RPATH='$ORIGIN'
  log_command "${logfile}" cmake --build "${build_dir}" --parallel "${JOBS}"
  log_command "${logfile}" cmake --install "${build_dir}"
}

build_libmatroska() {
  local source_dir="${SOURCE_ROOT}/libmatroska"
  local build_dir="${BUILD_ROOT_DIR}/libmatroska-${LIBMATROSKA_VERSION}"
  local logfile="${LOG_DIR}/libmatroska.log"
  [[ -f "${source_dir}/CMakeLists.txt" ]] || fail "Execute 'bash scripts/fetch_sources.sh libmatroska' primeiro."
  local ebml_config
  ebml_config="$(find "${DIST_PREFIX}" -type f -name EBMLConfig.cmake -print -quit)"
  [[ -n "${ebml_config}" ]] || fail "libebml não está instalada em ${DIST_PREFIX}; compile-a primeiro."
  log_command "${logfile}" cmake -S "${source_dir}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${DIST_PREFIX}" \
    -DBUILD_SHARED_LIBS=ON -DEBML_DIR="$(dirname -- "${ebml_config}")" \
    -DCMAKE_INSTALL_RPATH='$ORIGIN'
  log_command "${logfile}" cmake --build "${build_dir}" --parallel "${JOBS}"
  log_command "${logfile}" cmake --install "${build_dir}"
}

target="${1:-all}"
case "${target}" in
  libebml|ebml) build_libebml ;;
  libmatroska|matroska) build_libmatroska ;;
  all)
    build_libebml
    build_libmatroska
    ;;
  *)
    printf 'Uso: bash scripts/build_matroska.sh [libebml|libmatroska|all]\n' >&2
    exit 2
    ;;
esac
