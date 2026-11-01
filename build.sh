#!/usr/bin/env bash
#
# Baut PicoBoy Duo fuer den (alten) PicoBoy mit dem Pico SDK.
#
#   ./build.sh          bauen
#   ./build.sh clean    Bauordner loeschen und neu bauen
#
# Ergebnis: dist/pbduo.uf2
set -euo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${PICO_SDK_PATH:-}" ]; then
    for c in "${HOME}/pico/pico-sdk" "${HOME}/pico-sdk"; do
        [ -d "${c}" ] && { export PICO_SDK_PATH="${c}"; break; }
    done
fi
[ -n "${PICO_SDK_PATH:-}" ] || { echo "PICO_SDK_PATH ist nicht gesetzt." >&2; exit 1; }

[ "${1:-}" = clean ] && rm -rf "${HIER}/build"
mkdir -p "${HIER}/build" "${HIER}/dist"
cmake -S "${HIER}" -B "${HIER}/build" -DPICO_SDK_PATH="${PICO_SDK_PATH}" > /dev/null
cmake --build "${HIER}/build" -j"$(nproc)"
cp "${HIER}/build/pbduo.uf2" "${HIER}/dist/pbduo.uf2"
echo "fertig: dist/pbduo.uf2"
