#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/build_common.sh"
require_commands pkg-config find patchelf
setup_native_pkg_config
SOURCE_DIR="${SOURCE_ROOT}/mkvtoolnix-${MKVTOOLNIX_VERSION}"
MKV_BUILD="${BUILD_ROOT_DIR}/mkvtoolnix-${MKVTOOLNIX_VERSION}"
LOGFILE="${LOG_DIR}/mkvtoolnix-${MKVTOOLNIX_VERSION}.log"
[[ -d "${SOURCE_DIR}" ]] || fail "Execute fetch_sources.sh primeiro."

if [[ ! -x "${SOURCE_DIR}/configure" ]]; then
  [[ -x "${SOURCE_DIR}/autogen.sh" ]] || fail "Fonte sem configure nem autogen.sh."
  log_command "${LOGFILE}" bash -c "cd \"${SOURCE_DIR}\" && ./autogen.sh"
fi
mkdir -p "${MKV_BUILD}"
"${SOURCE_DIR}/configure" --help >"${MKV_BUILD}/configure-help.txt"
grep -q -- '--enable-gui' "${MKV_BUILD}/configure-help.txt" ||
  fail "A fonte não anuncia --enable-gui; não é possível confirmar o controle da GUI."

export CPPFLAGS="-I${DIST_PREFIX}/include ${CPPFLAGS:-}"
export LDFLAGS="-L${DIST_PREFIX}/lib ${LDFLAGS:-}"
export PKG_CONFIG_PATH="${DIST_PREFIX}/lib/pkgconfig:${DIST_PREFIX}/lib64/pkgconfig:${DIST_PREFIX}/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
(
  cd "${SOURCE_DIR}"
  log_command "${LOGFILE}" ./configure --prefix="${DIST_PREFIX}" --enable-gui=no
  log_command "${LOGFILE}" ./drake --threads "${JOBS}" V=1 apps:cli
  log_command "${LOGFILE}" ./drake install
)
while IFS= read -r -d '' mkv_executable; do
  patchelf --set-rpath '$ORIGIN/../lib' "${mkv_executable}"
done < <(find "${DIST_PREFIX}/bin" -maxdepth 1 -type f -name 'mkv*' -print0)
"${DIST_PREFIX}/bin/mkvmerge" --version | tee -a "${LOGFILE}"
