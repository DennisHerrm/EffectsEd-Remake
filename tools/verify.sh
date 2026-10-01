#!/bin/sh
# Alles pruefen, was sich ohne Windows pruefen laesst.
#
# Steht im Baum und nicht in einem Notizzettel: was nur ich kenne, laeuft
# nicht, wenn jemand anders am Quelltext arbeitet — und auch nicht, wenn ich
# es vergesse.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

echo "== Kern uebersetzen und Tests =="
# -isystem third_party: minimp3.h liegt dort. Das fehlte hier, und damit lief
# der Prueflauf seit dem Einbau der MP3-Wiedergabe ueberhaupt nicht mehr durch
# — er brach in der ERSTEN Zeile ab. Aufgefallen erst, als ich ihn beim
# Abgleich mit dem Original zum ersten Mal seit Langem wieder als Ganzes
# aufgerufen habe.
g++ -std=c++20 -O2 -Wall -Wextra -Wshadow -pthread -Iinclude -isystem third_party \
    src/*.cpp tests/tests.cpp -o /tmp/efxtests
/tmp/efxtests data | tail -3

echo
echo "== gui/app.cpp gegen ImGui =="
# bash, nicht sh: check_gui.sh benutzt PIPESTATUS, und das kennt dash nicht.
#
# Mit `sh` scheiterte es an einer "Bad substitution" — und die Meldung
# darunter behauptete dann "ImGui nicht vorhanden". Der Uebersetzungsprueferer
# fuer gui/ wurde also stillschweigend uebersprungen, mit einer Begruendung,
# die nicht stimmte. Eine falsche Entwarnung ist schlimmer als eine fehlende
# Pruefung.
if ! bash tools/check_gui.sh; then
    echo "  FEHLGESCHLAGEN oder ImGui nicht vorhanden - siehe oben"
fi

echo
echo "== Abgleich mit dem Original =="
python3 tools/check_menu.py || true

echo
echo "== Pruefskripte =="
for script in lint_i18n lint_members lint_includes lint_calls lint_ns lint_sources lint_gpu lint_version lint_unwired lint_gl lint_std lint_win7; do
    printf "  %-16s " "$script"
    python3 "tools/$script.py" 2>&1 | tail -1
done
