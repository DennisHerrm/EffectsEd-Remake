#!/usr/bin/env bash
# Der Kern unter deutlich schaerferen Warnungen als im normalen Bau.
#
# Meldet, statt zu blockieren: nicht jede Meldung ist ein Fehler. Eine
# Schleifenvariable, die von 0 bis 63 laeuft und dann als Index dient, ist
# harmlos — der Uebersetzer weiss das nur nicht.
#
# Der Wert liegt darin, die ZAHL im Blick zu behalten. Steigt sie sprunghaft,
# ist an einer neuen Stelle etwas passiert, das man ansehen sollte.
set -u
cd "$(dirname "$0")/.."
echo "== Kern unter schaerfsten Warnungen =="
g++ -std=c++20 -fsyntax-only -Iinclude -isystem third_party \
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wold-style-cast -Wdouble-promotion -Wformat=2 -Wnull-dereference \
    -Wcast-qual -Wcast-align -Wduplicated-cond -Wduplicated-branches \
    -Wlogical-op src/*.cpp 2>&1 | grep -oE "\[-W[a-z-]+\]" | sort | uniq -c | sort -rn
echo
echo "  (Meldungen, kein Fehler — siehe Kopf der Datei)"
