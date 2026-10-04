#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/build_common.sh"
require_commands curl gpg tar xz
LOGFILE="${LOG_DIR}/fetch_sources.log"
STATUS_DIR="${BUILD_ROOT}/verification"
GNUPG_HOME="${BUILD_ROOT}/gnupg"
mkdir -p "${STATUS_DIR}" "${GNUPG_HOME}"
chmod 700 "${GNUPG_HOME}"
exec > >(tee -a "${LOGFILE}") 2>&1

download_if_missing() {
  local url="$1" output="$2"
  if [[ ! -s "${output}" ]]; then
    printf 'Baixando %s\n' "${url}"
    curl --fail --location --retry 3 --output "${output}" "${url}"
  fi
}

fetch_key() {
  local url="$1" name="$2"
  local key="${STATUS_DIR}/${name}.asc"
  download_if_missing "${url}" "${key}"
  gpg --homedir "${GNUPG_HOME}" --batch --import "${key}"
}

verify_signature() {
  local label="$1" signature="$2" archive="$3" expected="$4"
  local status="${STATUS_DIR}/${label}.signature.status"
  if ! gpg --homedir "${GNUPG_HOME}" --batch --status-fd 1 \
      --verify "${signature}" "${archive}" >"${status}" 2>&1; then
    cat "${status}" >&2
    fail "Assinatura inválida para ${label}; interrompi antes da extração e do build."
  fi
  local signer primary
  read -r signer primary < <(
    awk '$1 == "[GNUPG:]" && $2 == "VALIDSIG" { print toupper($3), toupper($NF); exit }' "${status}"
  )
  if [[ -z "${signer}" || ( "${signer}" != "${expected}" && "${primary}" != "${expected}" ) ]]; then
    cat "${status}" >&2
    fail "Signatário não aprovado para ${label}: recebido '${signer}', esperado '${expected}'."
  fi
  printf 'Assinatura verificada: %s (subchave %s; chave primária %s).\n' "${label}" "${signer}" "${primary}"
}

extract_verified_archive() {
  local archive="$1" destination="$2" expected_content="$3" temp_dir
  if [[ -e "${destination}" ]]; then
    case "${expected_content}" in
      cmake) [[ -f "${destination}/CMakeLists.txt" ]] ;;
      configure) [[ -x "${destination}/configure" ]] ;;
      mkvtoolnix) [[ -x "${destination}/configure" || -x "${destination}/autogen.sh" ]] ;;
    esac || fail "Diretório de fonte existente, mas incompleto: ${destination}"
    printf 'Fonte já extraída: %s\n' "${destination}"
    return
  fi
  temp_dir="$(mktemp -d "${SOURCE_ROOT}/.extract.XXXXXX")"
  tar -xJf "${archive}" --strip-components=1 -C "${temp_dir}"
  case "${expected_content}" in
    cmake) [[ -f "${temp_dir}/CMakeLists.txt" ]] ;;
    configure) [[ -x "${temp_dir}/configure" ]] ;;
    mkvtoolnix) [[ -x "${temp_dir}/configure" || -x "${temp_dir}/autogen.sh" ]] ;;
  esac || fail "Conteúdo esperado não encontrado ao extrair ${archive}"
  mv "${temp_dir}" "${destination}"
  printf 'Fonte extraída: %s\n' "${destination}"
}

fetch_signed_archive() {
  local label="$1" archive_url="$2" signature_url="$3" archive="$4" signature="$5"
  local expected_signer="$6" destination="$7" expected_content="$8"
  local key_url="$9" key_name="${10}"
  fetch_key "${key_url}" "${key_name}"
  download_if_missing "${signature_url}" "${signature}"
  download_if_missing "${archive_url}" "${archive}"
  verify_signature "${label}" "${signature}" "${archive}" "${expected_signer}"
  extract_verified_archive "${archive}" "${destination}" "${expected_content}"
}

fetch_one() {
  local component="$1"
  case "${component}" in
    libebml)
      fetch_signed_archive \
        "libebml-${LIBEBML_VERSION}" "${LIBEBML_SOURCE_URL}" "${LIBEBML_SOURCE_URL}.asc" \
        "${SOURCE_ROOT}/libebml-${LIBEBML_VERSION}.tar.xz" \
        "${SOURCE_ROOT}/libebml-${LIBEBML_VERSION}.tar.xz.asc" \
        "${MATROSKA_SIGNING_FINGERPRINT}" "${SOURCE_ROOT}/libebml" "cmake" \
        "https://mkvtoolnix.download/gpg-pub-moritzbunkus.txt" "matroska"
      ;;
    libmatroska)
      fetch_signed_archive \
        "libmatroska-${LIBMATROSKA_VERSION}" "${LIBMATROSKA_SOURCE_URL}" "${LIBMATROSKA_SOURCE_URL}.asc" \
        "${SOURCE_ROOT}/libmatroska-${LIBMATROSKA_VERSION}.tar.xz" \
        "${SOURCE_ROOT}/libmatroska-${LIBMATROSKA_VERSION}.tar.xz.asc" \
        "${MATROSKA_SIGNING_FINGERPRINT}" "${SOURCE_ROOT}/libmatroska" "cmake" \
        "https://mkvtoolnix.download/gpg-pub-moritzbunkus.txt" "matroska"
      ;;
    ffmpeg)
      fetch_signed_archive \
        "FFmpeg-${FFMPEG_VERSION}" "${FFMPEG_SOURCE_URL}" "${FFMPEG_SOURCE_URL}.asc" \
        "${SOURCE_ROOT}/ffmpeg-${FFMPEG_VERSION}.tar.xz" \
        "${SOURCE_ROOT}/ffmpeg-${FFMPEG_VERSION}.tar.xz.asc" \
        "${FFMPEG_SIGNING_FINGERPRINT}" "${SOURCE_ROOT}/ffmpeg-${FFMPEG_VERSION}" "configure" \
        "https://ffmpeg.org/ffmpeg-devel.asc" "ffmpeg-release"
      ;;
    mkvtoolnix)
      fetch_signed_archive \
        "MKVToolNix-${MKVTOOLNIX_VERSION}" "${MKVTOOLNIX_SOURCE_URL}" "${MKVTOOLNIX_SOURCE_URL}.sig" \
        "${SOURCE_ROOT}/mkvtoolnix-${MKVTOOLNIX_VERSION}.tar.xz" \
        "${SOURCE_ROOT}/mkvtoolnix-${MKVTOOLNIX_VERSION}.tar.xz.sig" \
        "${MKVTOOLNIX_SIGNING_FINGERPRINT}" "${SOURCE_ROOT}/mkvtoolnix-${MKVTOOLNIX_VERSION}" "mkvtoolnix" \
        "https://mkvtoolnix.download/gpg-pub-moritzbunkus.txt" "mkvtoolnix-release"
      ;;
    *)
      fail "Componente inválido '${component}'. Use: libebml, libmatroska, ffmpeg ou mkvtoolnix."
      ;;
  esac
}

[[ $# -eq 1 ]] || fail "Informe um único componente para manter a verificação e a ordem: libebml, libmatroska, ffmpeg ou mkvtoolnix."
fetch_one "$1"
