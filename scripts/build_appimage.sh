#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
DIST_ROOT="${REPO_ROOT}/dist/linux"
APPDIR="${REPO_ROOT}/build/Musubi.AppDir"
OUTPUT="${REPO_ROOT}/dist/Musubi-x86_64.AppImage"

command -v appimagetool >/dev/null 2>&1 || {
  printf 'ERRO: appimagetool não encontrado. Instale-o para empacotar o AppImage.\n' >&2
  exit 2
}
command -v linuxdeploy >/dev/null 2>&1 || {
  printf 'ERRO: linuxdeploy com o plugin Qt não encontrado. Instale-o para empacotar Qt e suas dependências.\n' >&2
  exit 2
}
[[ -x "${DIST_ROOT}/Musubi" ]] || {
  printf 'ERRO: compile e instale Musubi em dist/linux antes de empacotar.\n' >&2
  exit 2
}

rm -rf -- "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/icons/hicolor/512x512/apps" "${APPDIR}/usr/lib"
cp "${DIST_ROOT}/Musubi" "${APPDIR}/usr/bin/Musubi"
cp "${REPO_ROOT}/cpp/resources/linux/musubi.desktop" "${APPDIR}/usr/share/applications/musubi.desktop"
cp "${REPO_ROOT}/cpp/resources/icons/png/512x512/musubi.png" "${APPDIR}/usr/share/icons/hicolor/512x512/apps/musubi.png"
if [[ -d "${DIST_ROOT}/lib" ]]; then cp -a "${DIST_ROOT}/lib/." "${APPDIR}/usr/lib/"; fi
if [[ -d "${DIST_ROOT}/bin" ]]; then cp -a "${DIST_ROOT}/bin/." "${APPDIR}/usr/bin/"; fi

cat >"${APPDIR}/AppRun" <<'APPRUN'
#!/usr/bin/env bash
HERE="$(dirname -- "$(readlink -f -- "$0")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${HERE}/usr/bin/Musubi" "$@"
APPRUN
chmod +x "${APPDIR}/AppRun"
ln -sf usr/share/applications/musubi.desktop "${APPDIR}/musubi.desktop"
ln -sf usr/share/icons/hicolor/512x512/apps/musubi.png "${APPDIR}/musubi.png"
linuxdeploy --appdir "${APPDIR}" --plugin qt
mkdir -p "$(dirname -- "${OUTPUT}")"
ARCH=x86_64 appimagetool "${APPDIR}" "${OUTPUT}"
printf 'AppImage criado: %s\n' "${OUTPUT}"
