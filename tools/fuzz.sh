#!/usr/bin/env bash
# Alle Zerleger gegen verbogene und zufaellige Eingaben, unter Address- und
# UB-Sanitizer.
#
# Die Testreihe prueft, ob die Zerleger das RICHTIGE tun. Das hier prueft, ob
# sie sich bei falschen Eingaben nichts zuschulden kommen lassen — das ist
# eine andere Frage, und bei Dateien aus dem Netz die wichtigere.
#
# Laeuft nicht bei jedem Bau: Sanitizer machen den Durchlauf zehnmal langsamer.
# Vor einer Veroeffentlichung aber schon.
set -eu
cd "$(dirname "$0")/.."
ROUNDS="${1:-4000}"

echo "== Zerleger unter Sanitizer =="
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Iinclude -Ithird_party src/*.cpp tools/fuzz_main.cpp -o /tmp/efx_fuzz
/tmp/efx_fuzz "$ROUNDS"
