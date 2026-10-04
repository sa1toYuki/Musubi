#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/build_common.sh"
require_commands make find patchelf readelf
setup_native_pkg_config
SOURCE_DIR="${SOURCE_ROOT}/ffmpeg-${FFMPEG_VERSION}"
FFMPEG_BUILD="${BUILD_ROOT_DIR}/ffmpeg-${FFMPEG_VERSION}"
LOGFILE="${LOG_DIR}/ffmpeg-${FFMPEG_VERSION}.log"
[[ -x "${SOURCE_DIR}/configure" ]] || fail "Execute fetch_sources.sh primeiro."
mkdir -p "${FFMPEG_BUILD}"
(
  cd "${FFMPEG_BUILD}"
  log_command "${LOGFILE}" "${SOURCE_DIR}/configure" \
    --prefix="${DIST_PREFIX}" --bindir="${DIST_PREFIX}/bin" --libdir="${DIST_PREFIX}/lib" \
    --enable-shared --disable-static --enable-pic --disable-autodetect --extra-cflags=-fPIC
  if grep -Eq '^CONFIG_(GPL|VERSION3|NONFREE)=yes$' config.mak; then
    fail "Configuração FFmpeg ativou opção GPL, version3 ou nonfree."
  fi
  log_command "${LOGFILE}" make --jobs="${JOBS}"
  log_command "${LOGFILE}" make install
)
for executable in ffmpeg ffprobe; do
  patchelf --set-rpath '$ORIGIN/../lib' "${DIST_PREFIX}/bin/${executable}"
done
while IFS= read -r -d '' shared_library; do
  patchelf --set-rpath '$ORIGIN' "${shared_library}"
done < <(find "${DIST_PREFIX}/lib" -maxdepth 1 -type f -name 'lib*.so.*' -print0)
readelf -d "${DIST_PREFIX}/bin/ffmpeg" | grep -F '$ORIGIN/../lib' >/dev/null \
  || fail "RPATH do FFmpeg não contém \$ORIGIN/../lib."
"${DIST_PREFIX}/bin/ffmpeg" -version | tee -a "${LOGFILE}"
"${DIST_PREFIX}/bin/ffprobe" -version | tee -a "${LOGFILE}"
if command -v ldd >/dev/null 2>&1; then
  ldd "${DIST_PREFIX}/bin/ffmpeg" | tee -a "${LOGFILE}"
  ldd "${DIST_PREFIX}/bin/ffprobe" | tee -a "${LOGFILE}"
fi
