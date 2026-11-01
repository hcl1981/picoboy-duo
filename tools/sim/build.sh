#!/usr/bin/env bash
# Baut den PC-Simulator (mit AddressSanitizer/UBSan) aus den echten Quellen.
set -euo pipefail
HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${HIER}/../../src"
ROOT="${HIER}/../.."
U8G2="${ROOT}/third_party/u8g2"
# U8g2 (C) und die PicoPac-Spiele (Warnungen aus - Originalcode) vorab uebersetzen
SIMOBJ="${HIER}/obj"; mkdir -p "${SIMOBJ}"; rm -f "${SIMOBJ}"/*.o
for f in "${U8G2}"/*.c; do
    gcc -O1 -g -w -fsanitize=address,undefined -c "$f" -I"${U8G2}" -o "${SIMOBJ}/$(basename "$f" .c).o"
done
g++ -std=c++17 -O1 -g -w -fsanitize=address,undefined -I"${HIER}/include" -I"${SRC}" -I"${SRC}/picopac" \
    -I"${ROOT}/picopac" -I"${U8G2}" -c "${SRC}/picopac/picopac.cpp" -o "${SIMOBJ}/picopac.o"
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wall -Wno-unused-function \
    -I"${HIER}/include" -I"${SRC}" -I"${SRC}/picopac" -I"${ROOT}/picopac" -I"${ROOT}/jumpnbump/src" -I"${U8G2}" \
    "${HIER}/sim.cpp" "${SRC}/gfx.cpp" "${SRC}/play.cpp" "${SRC}"/games/*.cpp \
    "${SRC}/multi.cpp" "${SRC}/settings.cpp" "${SRC}/starscreen.cpp" "${ROOT}"/jumpnbump/src/*.cpp \
    "${SRC}/picopac/arduino.cpp" "${SIMOBJ}"/*.o \
    -x c++ <(sed 's/^int main()/int pb_main()/' "${SRC}/main.cpp") -x none \
    -o "${HIER}/pbsim"
echo "ok: ${HIER}/pbsim"
